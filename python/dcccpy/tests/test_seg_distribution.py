from __future__ import annotations

import hashlib
import importlib.util
from pathlib import Path
import zipfile

import pytest


FILES = ("dccc_seg.onnx", "manifest.json", "grid.json", "plans.json", "dataset.json",
         "labels.json", "neurosegmenter_mapping.json", "validation.json")


def import_script(path):
    spec = importlib.util.spec_from_file_location("release_script", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


@pytest.fixture
def distribution(tmp_path, monkeypatch):
    import dcccpy.segmentation as model
    archive = tmp_path / "model.zip"
    with zipfile.ZipFile(archive, "w") as z:
        for name in FILES:
            z.writestr("seg/" + name, b"model" if name.endswith(".onnx") else b"{}")
    monkeypatch.setenv("DCCCPY_CACHE_DIR", str(tmp_path / "cache"))
    monkeypatch.setenv("DCCCPY_SEG_MODEL_URL", archive.as_uri())
    monkeypatch.setattr(model, "SEG_MODEL_SHA256", hashlib.sha256(b"model").hexdigest())
    return model, archive


def test_model_download_is_cached_outside_package(distribution):
    model, archive = distribution
    directory = model.download_seg_model()
    assert directory.name == "4.6.1-alpha"
    assert (directory / "dccc_seg.onnx").read_bytes() == b"model"
    archive.unlink()
    assert model.download_seg_model() == directory


def test_model_download_verifies_the_bound_noflip_bundle(tmp_path, monkeypatch):
    import dcccpy.segmentation as model
    root = Path(__file__).resolve().parents[3]
    bundle = root / "localizer/src/assets/models/seg"
    packer = import_script(root / "scripts/package-seg-model.py")
    archive = packer.package_model(bundle, tmp_path / "release", "4.6.1-alpha")
    cache = tmp_path / "cache"
    old = cache / "models/seg/4.6.0-alpha"
    old.mkdir(parents=True)
    (old / "dccc_seg.onnx").write_bytes(b"older release; must never be reused or altered")
    monkeypatch.setenv("DCCCPY_CACHE_DIR", str(cache))
    monkeypatch.setenv("DCCCPY_SEG_MODEL_URL", archive.as_uri())
    downloaded = model.download_seg_model()
    assert downloaded == cache / "models/seg/4.6.1-alpha"
    assert hashlib.sha256((downloaded / "dccc_seg.onnx").read_bytes()).hexdigest() == (
        "a60ad8bcd4f6c6e1fd0fc739784da392ff5d65eafeed1e6091c4608ef28db81e")
    assert (old / "dccc_seg.onnx").read_bytes() == b"older release; must never be reused or altered"


@pytest.mark.parametrize("kind", ["digest", "traversal", "missing"])
def test_model_download_rejects_bad_assets(distribution, kind, tmp_path):
    model, archive = distribution
    with zipfile.ZipFile(archive, "w") as z:
        for name in FILES:
            if kind == "missing" and name == "labels.json":
                continue
            z.writestr("seg/" + name, b"wrong" if kind == "digest" else
                       (b"model" if name.endswith(".onnx") else b"{}"))
        if kind == "traversal":
            z.writestr("../escaped", b"bad")
    with pytest.raises(model.SegModelDownloadError):
        model.download_seg_model()
    assert not (tmp_path / "escaped").exists()
    assert not model.seg_model_dir().exists()


def test_bundled_model_and_download_opt_out(distribution, monkeypatch, tmp_path):
    model, archive = distribution
    exe = tmp_path / "native" / "DCCCcore"
    bundle = exe.parent / "assets/models/seg"
    bundle.mkdir(parents=True)
    for name in FILES:
        (bundle / name).write_bytes(b"model" if name.endswith(".onnx") else b"{}")
    monkeypatch.setenv("DCCCPY_AUTO_DOWNLOAD", "0")
    assert model.default_seg_bundle(exe) is None
    (bundle / "dccc_seg.onnx").unlink()
    with pytest.raises(model.SegModelDownloadError, match="download"):
        model.default_seg_bundle(exe)
    cached = model.download_seg_model()  # Explicit provisioning honors user's call.
    assert model.default_seg_bundle(exe) == cached


def test_native_symlink_reuses_real_bundled_model(distribution, monkeypatch, tmp_path):
    model, _ = distribution
    native = tmp_path / "native/DCCCcore"
    bundle = native.parent / "assets/models/seg"
    bundle.mkdir(parents=True)
    native.write_text("native executable")
    for name in FILES:
        (bundle / name).write_bytes(b"model" if name.endswith(".onnx") else b"{}")
    link = tmp_path / "bin/DCCCcore"
    link.parent.mkdir()
    link.symlink_to(native)
    monkeypatch.setenv("DCCCPY_AUTO_DOWNLOAD", "0")
    assert model.default_seg_bundle(link) is None


def test_native_old_checkpoint_is_rejected_instead_of_silently_reused(distribution, tmp_path):
    model, _ = distribution
    exe = tmp_path / "old-native/DCCCcore"
    bundle = exe.parent / "assets/models/seg"
    bundle.mkdir(parents=True)
    for name in FILES:
        (bundle / name).write_bytes(b"older checkpoint" if name.endswith(".onnx") else b"{}")
    with pytest.raises(model.SegModelDownloadError, match="incompatible"):
        model.default_seg_bundle(exe)


@pytest.mark.parametrize("package", ["linux", "linux-arm64", "macos", "windows"])
def test_every_runtime_profile_excludes_seg(tmp_path, package):
    root = Path(__file__).resolve().parents[3]
    vendor = import_script(root / f"python/dcccpy-{package}-runtime/scripts/vendor_dccccore.py")
    for profile in vendor.EXCLUDE_PROFILES:
        dest = tmp_path / profile
        model = dest / "assets/models/seg/dccc_seg.onnx"
        rigid = dest / "assets/models/registration/rigid.onnx"
        model.parent.mkdir(parents=True)
        rigid.parent.mkdir(parents=True)
        model.write_bytes(b"bulky optional segmentation")
        rigid.write_bytes(b"required rigid")
        vendor.prune_profile(dest, profile)
        assert not model.exists()
        assert rigid.exists()


def test_model_release_zip_contains_runtime_controls(tmp_path):
    root = Path(__file__).resolve().parents[3]
    packer = import_script(root / "scripts/package-seg-model.py")
    bundle = tmp_path / "source"
    bundle.mkdir()
    for name in FILES:
        (bundle / name).write_bytes(b"model" if name.endswith(".onnx") else b"{}")
    archive = packer.package_model(bundle, tmp_path / "dist", "4.6.0-alpha")
    assert archive.name == "DCCCseg-4.6.0-alpha.zip"
    with zipfile.ZipFile(archive) as z:
        assert set(z.namelist()) == {"seg/" + name for name in FILES}


def test_model_release_preserves_reference_entrypoint(tmp_path):
    root = Path(__file__).resolve().parents[3]
    packer = import_script(root / "scripts/package-seg-model.py")
    bundle = tmp_path / "source"
    bundle.mkdir()
    for name in FILES:
        (bundle / name).write_bytes(b"model" if name.endswith(".onnx") else b"{}")
    (bundle / "README.md").write_text("See dccc_seg.py for reference inference")
    (bundle / "dccc_seg.py").write_text("# shipped reference entry point\n")
    (bundle / "file_manifest.json").write_text('{"files": []}')
    archive = packer.package_model(bundle, tmp_path / "dist", "4.6.0-alpha")
    with zipfile.ZipFile(archive) as z:
        assert z.read("seg/dccc_seg.py") == b"# shipped reference entry point\n"
        assert z.read("seg/file_manifest.json") == b'{"files": []}'


def test_release_version_must_match_source():
    root = Path(__file__).resolve().parents[3]
    metadata = import_script(root / "scripts/release-metadata.py")
    cmake = 'project(DCCCcore VERSION 4.6.0)\nset(DCCCCORE_VERSION_SUFFIX "-alpha")\n'
    assert metadata.release_version(cmake, requested="4.6.0-alpha") == "4.6.0-alpha"
    assert metadata.release_version(cmake, tag="v4.6.0-alpha") == "4.6.0-alpha"
    with pytest.raises(ValueError):
        metadata.release_version(cmake, requested="4.5.1-alpha")
    with pytest.raises(ValueError):
        metadata.release_version(cmake, tag="v4.6.0")
