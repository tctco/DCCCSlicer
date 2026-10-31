from __future__ import annotations

import os
import hashlib
import subprocess
from pathlib import Path
import sys
import types
import zipfile

import pytest

import dcccpy
from dcccpy.runtime import (
    DCCCCORE_VERSION,
    DCCCcoreNotFoundError,
    dccccore_path,
    find_existing_dccccore,
    release_platform,
    release_url,
)


def make_fake_dccccore(tmp_path: Path, monkeypatch=None, *, version=DCCCCORE_VERSION) -> Path:
    if monkeypatch is not None:
        import dcccpy.segmentation as model
        monkeypatch.setattr(model, "SEG_MODEL_SHA256", hashlib.sha256(b"test runtime resource").hexdigest())
    bundle = tmp_path / "assets/models/seg"
    bundle.mkdir(parents=True, exist_ok=True)
    for name in ("dccc_seg.onnx", "manifest.json", "grid.json", "plans.json", "dataset.json",
                 "labels.json", "neurosegmenter_mapping.json", "validation.json"):
        (bundle / name).write_bytes(b"test runtime resource")
    exe = tmp_path / "DCCCcore"
    exe.write_text(
        "#!/usr/bin/env sh\n"
        f"if [ \"$1\" = '--version' ]; then printf '%s\\n' '{version}'; exit 0; fi\n"
        "printf '%s\\n' \"$@\" > \"$DCCCPY_FAKE_ARGS\"\n"
        "out=''\n"
        "while [ \"$#\" -gt 0 ]; do\n"
        "  if [ \"$1\" = '--output' ]; then shift; out=\"$1\"; fi\n"
        "  shift || true\n"
        "done\n"
        "if [ -n \"$out\" ]; then mkdir -p \"$(dirname \"$out\")\"; printf 'fake' > \"$out\"; fi\n"
        "printf 'Metric: Centiloid\\n  fbp: 12.5\\n  SUVr: 1.23\\n'\n"
    )
    exe.chmod(0o755)
    return exe


def test_parse_metrics() -> None:
    metrics = dcccpy.parse_metrics(
        "Metric: Centiloid\n"
        "  fbp: 12.5\n"
        "  SUVr: 1.23\n"
        "ADAD score: -0.2\n"
    )
    assert metrics == {"fbp": 12.5, "SUVr": 1.23, "ADAD score": -0.2}


def test_seg_defaults_and_explicit_options(tmp_path, monkeypatch):
    fake_args = tmp_path / "args.txt"
    fake_exe = make_fake_dccccore(tmp_path, monkeypatch)
    monkeypatch.setenv("DCCCPY_FAKE_ARGS", str(fake_args))
    result = dcccpy.seg("pet.nii", executable=fake_exe)
    args = fake_args.read_text().splitlines()
    assert args[0] == "seg"
    assert args[args.index("--mode") + 1] == "freesurfer"
    assert args[args.index("--threads") + 1] == "4"
    assert "--no-tta" not in args
    assert result.output.exists()
    result = dcccpy.seg("pet.nii", tmp_path / "labels.nii.gz", mode="training",
                        threads=2, bundle="model bundle",
                        rigid_header="rigid header.nii.gz", config="config.toml",
                        debug=True, executable=fake_exe)
    args = fake_args.read_text().splitlines()
    assert args[args.index("--mode") + 1] == "training"
    assert args[args.index("--threads") + 1] == "2"
    assert args[args.index("--bundle") + 1] == str(Path("model bundle").resolve())
    assert args[args.index("--rigid-header") + 1] == str(Path("rigid header.nii.gz").resolve())
    assert args[args.index("--config") + 1] == str(Path("config.toml").resolve())
    assert "--debug" in args and "--no-tta" not in args


@pytest.mark.parametrize("tta", [True, False])
def test_seg_rejects_legacy_tta_keyword_before_execution(tmp_path, monkeypatch, tta):
    exe = make_fake_dccccore(tmp_path)
    args_file = tmp_path / "never-spawned.txt"
    monkeypatch.setenv("DCCCPY_FAKE_ARGS", str(args_file))
    with pytest.raises(TypeError, match="tta"):
        dcccpy.seg("pet.nii", tta=tta, executable=exe)
    assert not args_file.exists()


@pytest.mark.parametrize("override", ["config", "bundle"])
@pytest.mark.parametrize("version", ["4.6.1-alpha", "4.6.3-alpha", "unknown-version"])
def test_seg_requires_bound_native_version_even_with_model_overrides(tmp_path, monkeypatch, override, version):
    exe = make_fake_dccccore(tmp_path, version=version)
    args_file = tmp_path / "must-not-run-seg.txt"
    monkeypatch.setenv("DCCCPY_FAKE_ARGS", str(args_file))
    kwargs = {override: tmp_path / "custom", "executable": exe}
    with pytest.raises(dcccpy.SegModelDownloadError, match="4.6.2-alpha"):
        dcccpy.seg("pet.nii", **kwargs)
    assert not args_file.exists()


@pytest.mark.parametrize("kwargs", [{"mode": "bogus"}, {"threads": 0},
                                    {"threads": 1.5}, {"threads": True}])
def test_seg_rejects_bad_options(kwargs):
    with pytest.raises(ValueError):
        dcccpy.seg("pet.nii", **kwargs)


def test_seg_preserves_native_failure(tmp_path, monkeypatch):
    exe = make_fake_dccccore(tmp_path, monkeypatch)
    exe.write_text("#!/bin/sh\nif [ \"$1\" = '--version' ]; then printf '4.6.2-alpha\\n'; exit 0; fi\n"
                   "printf 'invalid segmentation model' >&2\nexit 7\n")
    exe.chmod(0o755)
    result = dcccpy.seg("pet.nii", check=False, executable=exe)
    assert result.returncode == 7
    assert "invalid segmentation model" in result.stderr
    assert not result.output.exists()
    with pytest.raises(subprocess.CalledProcessError) as error:
        dcccpy.seg("pet.nii", executable=exe)
    assert error.value.returncode == 7


def test_seg_custom_config_does_not_provision_or_override_model(tmp_path, monkeypatch):
    exe = make_fake_dccccore(tmp_path)
    (tmp_path / "assets/models/seg/dccc_seg.onnx").unlink()
    args_file = tmp_path / "args.txt"
    monkeypatch.setenv("DCCCPY_FAKE_ARGS", str(args_file))
    monkeypatch.setenv("DCCCPY_AUTO_DOWNLOAD", "0")
    monkeypatch.setenv("DCCCPY_CACHE_DIR", str(tmp_path / "empty-cache"))
    config = tmp_path / "custom.toml"
    config.write_text('[models]\nseg = "offline/custom/dccc_seg.onnx"\n')
    result = dcccpy.seg("pet.nii", config=config, executable=exe)
    assert result.returncode == 0
    args = args_file.read_text().splitlines()
    assert args[args.index("--config") + 1] == str(config)
    assert "--bundle" not in args


def test_centiloid_omits_output_by_creating_temp_path(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    fake_args = tmp_path / "args.txt"
    fake_exe = make_fake_dccccore(tmp_path)
    monkeypatch.setenv("DCCCPY_DCCCCORE", str(fake_exe))
    monkeypatch.setenv("DCCCPY_FAKE_ARGS", str(fake_args))

    result = dcccpy.centiloid("input.nii", skip_normalization=True)

    assert result.returncode == 0
    assert result.output is not None
    assert result.output.exists()
    assert result.temp_dir is not None
    assert result.output.parent == result.temp_dir
    assert result.metrics["fbp"] == 12.5

    args = fake_args.read_text().splitlines()
    assert args[:5] == [
        "centiloid",
        "--input",
        str(Path("input.nii").resolve()),
        "--output",
        str(result.output),
    ]
    assert "--skip-normalization" in args


def test_metric_helpers_resolve_relative_input_and_output_paths(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    work_dir = tmp_path / "work"
    work_dir.mkdir()
    (work_dir / "input.nii").write_text("fake input")

    fake_args = tmp_path / "args.txt"
    fake_exe = make_fake_dccccore(tmp_path)
    monkeypatch.chdir(work_dir)
    monkeypatch.setenv("DCCCPY_DCCCCORE", str(fake_exe))
    monkeypatch.setenv("DCCCPY_FAKE_ARGS", str(fake_args))

    result = dcccpy.centiloid("./input.nii", "./out/result.nii")

    expected_input = (work_dir / "input.nii").resolve()
    expected_output = (work_dir / "out" / "result.nii").resolve()
    assert result.output == expected_output
    assert expected_output.exists()

    args = fake_args.read_text().splitlines()
    assert args[:5] == [
        "centiloid",
        "--input",
        str(expected_input),
        "--output",
        str(expected_output),
    ]


def test_nibabel_like_input_is_saved_to_temp_file(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    class FakeImage:
        def __init__(self) -> None:
            self.saved_to: Path | None = None

        def to_filename(self, filename: str) -> None:
            self.saved_to = Path(filename)
            self.saved_to.write_text("fake nifti")

    fake_args = tmp_path / "args.txt"
    fake_exe = make_fake_dccccore(tmp_path)
    image = FakeImage()
    output = tmp_path / "output.nii"
    monkeypatch.setenv("DCCCPY_DCCCCORE", str(fake_exe))
    monkeypatch.setenv("DCCCPY_FAKE_ARGS", str(fake_args))

    result = dcccpy.centiloid(image, output)

    assert result.returncode == 0
    assert result.output == output
    assert result.temp_dir is not None
    assert image.saved_to is not None
    assert image.saved_to.exists()

    args = fake_args.read_text().splitlines()
    assert args[:5] == ["centiloid", "--input", str(image.saved_to), "--output", str(output)]


def test_dccccore_path_can_disable_auto_download(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("DCCCPY_DCCCCORE", raising=False)
    monkeypatch.setenv("DCCCPY_AUTO_DOWNLOAD", "0")
    monkeypatch.setenv("DCCCPY_CACHE_DIR", str(tmp_path / "cache"))
    monkeypatch.setenv("PATH", str(tmp_path))

    with pytest.raises(DCCCcoreNotFoundError):
        dccccore_path()


def test_dccccore_path_auto_downloads_to_cache(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    asset_root = tmp_path / "asset" / "DCCCcore-4.6.2-alpha-ubuntu-latest-x64"
    asset_root.mkdir(parents=True)
    exe = asset_root / "DCCCcore"
    exe.write_text("#!/usr/bin/env sh\nprintf 'fake dccccore\\n'\n")
    exe.chmod(0o755)

    archive = tmp_path / "DCCCcore.zip"
    with zipfile.ZipFile(archive, "w") as zf:
        zf.write(exe, "DCCCcore-4.6.2-alpha-ubuntu-latest-x64/DCCCcore")

    monkeypatch.delenv("DCCCPY_DCCCCORE", raising=False)
    monkeypatch.setenv("DCCCPY_AUTO_DOWNLOAD", "1")
    monkeypatch.setenv("DCCCPY_CACHE_DIR", str(tmp_path / "cache"))
    monkeypatch.setenv("DCCCPY_DCCCCORE_URL", archive.as_uri())
    monkeypatch.setenv("PATH", str(tmp_path))

    resolved = dccccore_path()

    assert resolved.exists()
    assert resolved.read_text().startswith("#!/usr/bin/env sh")
    assert str(tmp_path / "cache") in str(resolved)


def test_find_existing_dccccore_checks_macos_runtime_package(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    root = tmp_path / "macos-runtime"
    root.mkdir()
    exe = root / "DCCCcore"
    exe.write_text("#!/usr/bin/env sh\n")
    exe.chmod(0o755)

    module = types.SimpleNamespace(DCCCCORE_VERSION="4.6.2-alpha", dccccore_root=lambda: root)
    monkeypatch.setitem(sys.modules, "dcccpy_macos_runtime", module)
    monkeypatch.setenv("PATH", str(tmp_path / "empty-path"))

    assert find_existing_dccccore() == exe


def test_linux_arm64_release_platform() -> None:
    assert release_platform("linux-arm64") == "ubuntu-latest-arm64"


def test_default_release_targets_dccccore_461_alpha() -> None:
    assert DCCCCORE_VERSION == "4.6.2-alpha"
    assert release_url(platform_name="ubuntu-latest-x64").endswith(
        "/releases/download/v4.6.2-alpha/DCCCcore-4.6.2-alpha-ubuntu-latest-x64.zip"
    )


def test_alpha_ignores_incompatible_stable_runtime_package(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    root = tmp_path / "stable-runtime"
    root.mkdir()
    exe = root / "DCCCcore"
    exe.write_text("#!/usr/bin/env sh\n")
    exe.chmod(0o755)

    module = types.SimpleNamespace(DCCCCORE_VERSION="4.4.0", dccccore_root=lambda: root)
    monkeypatch.setitem(sys.modules, "dcccpy_linux_runtime", module)
    monkeypatch.setenv("PATH", str(tmp_path / "empty-path"))

    assert find_existing_dccccore() is None


def test_find_existing_dccccore_checks_linux_arm64_runtime_package(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    root = tmp_path / "linux-arm64-runtime"
    root.mkdir()
    exe = root / "DCCCcore"
    exe.write_text("#!/usr/bin/env sh\n")
    exe.chmod(0o755)

    module = types.SimpleNamespace(DCCCCORE_VERSION="4.6.2-alpha", dccccore_root=lambda: root)
    monkeypatch.setitem(sys.modules, "dcccpy_linux_arm64_runtime", module)
    monkeypatch.setenv("PATH", str(tmp_path / "empty-path"))

    assert find_existing_dccccore() == exe


def test_cli_forwards_raw_args(tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]) -> None:
    fake_args = tmp_path / "args.txt"
    fake_exe = make_fake_dccccore(tmp_path)
    monkeypatch.setenv("DCCCPY_DCCCCORE", str(fake_exe))
    monkeypatch.setenv("DCCCPY_FAKE_ARGS", str(fake_args))

    from dcccpy.cli import main

    code = main(["--help"])

    assert code == 0
    assert fake_args.read_text().splitlines() == ["--help"]
    assert "Metric: Centiloid" in capsys.readouterr().out


def test_pet_motion_correct_forwards_optional_outputs(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    fake_args = tmp_path / "args.txt"
    fake_exe = make_fake_dccccore(tmp_path)
    input_path = tmp_path / "dynamic.nii.gz"
    output_path = tmp_path / "average.nii.gz"
    corrected_path = tmp_path / "corrected.nii.gz"
    motion_path = tmp_path / "motion.tsv"
    input_path.write_text("fake dynamic PET")
    monkeypatch.setenv("DCCCPY_DCCCCORE", str(fake_exe))
    monkeypatch.setenv("DCCCPY_FAKE_ARGS", str(fake_args))

    result = dcccpy.pet_motion_correct(
        input_path,
        output_path,
        save_corrected_dynamic=corrected_path,
        motion_output=motion_path,
    )

    assert result.returncode == 0
    assert result.output == output_path
    assert output_path.exists()
    assert fake_args.read_text().splitlines() == [
        "pet-motion-correct",
        "--input",
        str(input_path),
        "--output",
        str(output_path),
        "--save-corrected-dynamic",
        str(corrected_path),
        "--motion-output",
        str(motion_path),
    ]


def test_adni_pet_core_requires_and_forwards_tracer(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    fake_args = tmp_path / "args.txt"
    fake_exe = make_fake_dccccore(tmp_path)
    input_path = tmp_path / "pet.nii"
    output_path = tmp_path / "adni.nii"
    input_path.write_text("fake PET")
    monkeypatch.setenv("DCCCPY_DCCCCORE", str(fake_exe))
    monkeypatch.setenv("DCCCPY_FAKE_ARGS", str(fake_args))

    result = dcccpy.adni_pet_core(input_path, output_path, tracer="fdg")

    assert result.returncode == 0
    assert fake_args.read_text().splitlines() == [
        "adni-pet-core",
        "--input",
        str(input_path),
        "--output",
        str(output_path),
        "--tracer",
        "fdg",
    ]


def test_adni_pet_core_python_api_has_no_default_tracer() -> None:
    with pytest.raises(TypeError, match="tracer"):
        dcccpy.adni_pet_core("pet.nii")


def test_adni_pet_core_forwards_combined_export_levels(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    fake_args = tmp_path / "args.txt"
    fake_exe = make_fake_dccccore(tmp_path)
    input_path = tmp_path / "pet.nii"
    output_path = tmp_path / "adni.nii"
    input_path.write_text("fake PET")
    monkeypatch.setenv("DCCCPY_DCCCCORE", str(fake_exe))
    monkeypatch.setenv("DCCCPY_FAKE_ARGS", str(fake_args))

    result = dcccpy.adni_pet_core(
        input_path, output_path, tracer="fdg", level=(1, 2)
    )

    assert result.returncode == 0
    assert fake_args.read_text().splitlines() == [
        "adni-pet-core",
        "--input",
        str(input_path),
        "--output",
        str(output_path),
        "--tracer",
        "fdg",
        "--level",
        "1",
        "2",
    ]


@pytest.mark.parametrize("level", [(), 0, 4, (1, 4)])
def test_adni_pet_core_rejects_invalid_export_levels(level: object) -> None:
    with pytest.raises(ValueError, match="level must contain"):
        dcccpy.adni_pet_core("pet.nii", tracer="fdg", level=level)  # type: ignore[arg-type]


def test_adni_pet_core_forwards_deface(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    fake_args = tmp_path / "args.txt"
    fake_exe = make_fake_dccccore(tmp_path)
    input_path = tmp_path / "pet.nii"
    output_path = tmp_path / "adni.nii"
    input_path.write_text("fake PET")
    monkeypatch.setenv("DCCCPY_DCCCCORE", str(fake_exe))
    monkeypatch.setenv("DCCCPY_FAKE_ARGS", str(fake_args))

    result = dcccpy.adni_pet_core(
        input_path, output_path, tracer="fdg", level=(1, 3), deface=True
    )

    assert result.returncode == 0
    assert fake_args.read_text().splitlines() == [
        "adni-pet-core",
        "--input",
        str(input_path),
        "--output",
        str(output_path),
        "--tracer",
        "fdg",
        "--level",
        "1",
        "3",
        "--deface",
    ]


def test_run_adds_macos_security_hint_for_blocked_runtime(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    exe = tmp_path / "DCCCcore"
    exe.write_text(
        "#!/usr/bin/env sh\n"
        "printf 'Operation not permitted\\n' >&2\n"
        "exit 126\n"
    )
    exe.chmod(0o755)
    monkeypatch.setenv("DCCCPY_DCCCCORE", str(exe))
    monkeypatch.setattr("dcccpy.core.sys.platform", "darwin")

    result = dcccpy.run(["--help"], check=False)

    assert result.returncode == 126
    assert "macOS may have blocked DCCCcore" in result.stderr
    assert "xattr -dr com.apple.quarantine" in result.stderr
    assert str(exe) in result.stderr


def test_scan_pet_core_forwards_fdg_levels_and_deface(tmp_path, monkeypatch):
    fake_args = tmp_path / "scan_args.txt"
    fake_exe = make_fake_dccccore(tmp_path)
    input_path = tmp_path / "fdg.nii"
    output_path = tmp_path / "scan.nii"
    input_path.write_text("fake PET")
    monkeypatch.setenv("DCCCPY_DCCCCORE", str(fake_exe))
    monkeypatch.setenv("DCCCPY_FAKE_ARGS", str(fake_args))
    result = dcccpy.scan_pet_core(input_path, output_path, tracer="fdg", level=(1, 3), deface=True)
    assert result.returncode == 0
    assert fake_args.read_text().splitlines() == [
        "scan-pet-core", "--input", str(input_path), "--output", str(output_path),
        "--tracer", "fdg", "--level", "1", "3", "--deface",
    ]


def test_scan_pet_core_requires_fdg():
    with pytest.raises(TypeError, match="tracer"):
        dcccpy.scan_pet_core("fdg.nii")
    with pytest.raises(ValueError, match="fdg"):
        dcccpy.scan_pet_core("pet.nii", tracer="abeta")


@pytest.mark.parametrize("level", [0, 4, (), (1, 4)])
def test_scan_pet_core_rejects_invalid_level(level):
    with pytest.raises(ValueError, match="level"):
        dcccpy.scan_pet_core("fdg.nii", tracer="fdg", level=level)
