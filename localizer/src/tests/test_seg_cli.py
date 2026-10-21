"""Segmentation entry point guards: no network execution needed for bad requests."""
import json
from pathlib import Path
import shutil

import pytest


def test_seg_help_describes_native_labels(run_subprocess):
    result = run_subprocess(["seg", "--help"])
    assert result.returncode == 0
    assert "--mode" in result.stdout
    assert "--no-tta" in result.stdout
    assert "--rigid-header" in result.stdout
    assert "pons" in result.stdout.lower()


@pytest.mark.parametrize("args, message", [
    (["--threads", "0"], "threads"),
    (["--mode", "bogus"], "mode"),
    (["--batch"], "not supported"),
])
def test_seg_rejects_invalid_options(run_subprocess, tmp_path, args, message):
    result = run_subprocess(["seg", "--input", "missing.nii", "--output",
                             str(tmp_path / "labels.nii.gz"), *args])
    assert result.returncode != 0
    assert message in (result.stdout + result.stderr).lower()


def test_seg_never_overwrites(run_subprocess, tmp_path):
    target = tmp_path / "existing.nii.gz"
    target.write_bytes(b"keep")
    result = run_subprocess(["seg", "--input", "missing.nii", "--output", str(target)])
    assert result.returncode != 0
    assert "exist" in (result.stdout + result.stderr).lower()
    assert target.read_bytes() == b"keep"


def test_seg_legacy_configuration_keeps_installed_bundle_default(run_subprocess, tmp_path):
    config = tmp_path / "legacy.toml"
    config.write_text('[models]\nrigid = "assets/models/registration/rigid.onnx"\n')
    result = run_subprocess(["seg", "--input", "missing.nii", "--output", str(tmp_path / "out.nii"),
                             "--config", str(config)])
    assert result.returncode != 0
    assert "manifest.json" not in result.stderr
    assert "missing.nii" in result.stderr


@pytest.mark.parametrize("kind, message", [("4d", "3-d"), ("empty", "positive"),
    ("nan", "finite"), ("inf64", "finite"), ("bad_rigid", "voxel indexing")])
def test_seg_rejects_invalid_images(run_subprocess, tmp_path, kind, message):
    import numpy as np
    import nibabel as nib
    data = np.ones((9, 10, 11, 2) if kind == "4d" else (9, 10, 11), dtype=np.float32)
    if kind == "empty": data.fill(0)
    if kind == "nan": data[1, 2, 3] = np.nan
    if kind == "inf64":
        data = data.astype(">f8")
        data[1, 2, 3] = np.inf
    native = tmp_path / "pet.nii.gz"
    header = None
    if kind == "inf64":
        header = nib.Nifti1Header(endianness=">")
        header.set_data_dtype(np.float64)
    nib.save(nib.Nifti1Image(data, np.eye(4), header), native)
    args = ["seg", "--input", str(native), "--output", str(tmp_path / "out.nii.gz")]
    if kind == "bad_rigid":
        rigid = tmp_path / "rigid.nii.gz"
        nib.save(nib.Nifti1Image(data * 2, np.eye(4)), rigid)
        args += ["--rigid-header", str(rigid)]
    result = run_subprocess(args)
    assert result.returncode != 0
    assert message in (result.stdout + result.stderr).lower()


@pytest.mark.parametrize("kind, message", [("count", "class count"),
    ("dataset", "dataset label mapping"), ("region", "regional label mapping")])
def test_seg_rejects_inconsistent_bundle(run_subprocess, tmp_path, kind, message):
    source = Path(__file__).parents[1] / "assets/models/seg"
    bundle = tmp_path / "bundle"
    bundle.mkdir()
    for path in source.glob("*.json"):
        shutil.copyfile(path, bundle / path.name)
    filename = {"count": "manifest.json", "dataset": "dataset.json", "region": "neurosegmenter_mapping.json"}[kind]
    metadata = json.loads((bundle / filename).read_text())
    if kind == "count": metadata["output"]["channels"] = 95
    if kind == "dataset": metadata["labels"]["background"] = 99
    if kind == "region": del metadata["raw_to_neuro"]["17"]
    (bundle / filename).write_text(json.dumps(metadata))
    (bundle / "dccc_seg.onnx").touch()
    result = run_subprocess(["seg", "--input", "missing.nii", "--output", str(tmp_path / "out.nii"), "--bundle", str(bundle)])
    assert result.returncode != 0
    assert message in result.stderr


@pytest.fixture(scope="module")
def constant_bundle(tmp_path_factory):
    """Cheap real ONNX graph: class 13 wins everywhere, independent of PET."""
    import numpy as np
    import onnx
    from onnx import helper, numpy_helper, TensorProto
    directory = tmp_path_factory.mktemp("seg-test-bundle")
    source = Path(__file__).parents[1] / "assets/models/seg"
    for path in source.glob("*.json"):
        shutil.copyfile(path, directory / path.name)
    biases = np.zeros((1, 96, 1, 1, 1), dtype=np.float32)
    biases[0, 13] = 1
    graph = helper.make_graph([helper.make_node("Expand", ["bias", "shape"], ["logits"])],
        "constant_seg", [helper.make_tensor_value_info("image", TensorProto.FLOAT, [1, 1, 128, 192, 128])],
        [helper.make_tensor_value_info("logits", TensorProto.FLOAT, [1, 96, 128, 192, 128])],
        [numpy_helper.from_array(biases, "bias"), numpy_helper.from_array(np.array([1, 96, 128, 192, 128], dtype=np.int64), "shape")])
    model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 17)], ir_version=9)
    onnx.save(model, directory / "dccc_seg.onnx")
    return directory


@pytest.mark.parametrize("mode, label, tta", [("training", 13, False),
    ("freesurfer", 17, True), ("neurosegmenter", 47, False)])
def test_seg_oblique_native_geometry_and_labels(run_subprocess, tmp_path, constant_bundle, mode, label, tta):
    import numpy as np
    import nibabel as nib
    from nibabel.processing import resample_from_to
    native_affine = np.array([[1.6, -.6, 0, -10], [.6, 1.6, 0, -30], [0, 0, 2, -10], [0, 0, 0, 1.]])
    rigid_affine = native_affine.copy()
    rigid_affine[:3, 3] += [10, -7, 3]
    data = np.arange(17 * 19 * 13, dtype=np.float32).reshape(17, 19, 13) + 1
    native = nib.Nifti1Image(data, native_affine)
    source, header, output = [tmp_path / name for name in ("pet.nii.gz", "rigid.nii.gz", "labels.nii.gz")]
    nib.save(native, source)
    nib.save(nib.Nifti1Image(data, rigid_affine), header)
    args = ["seg", "--input", str(source), "--output", str(output), "--rigid-header", str(header),
            "--bundle", str(constant_bundle), "--mode", mode, "--debug"]
    if not tta: args += ["--no-tta"]
    result = run_subprocess(args)
    assert result.returncode == 0, result.stdout + result.stderr
    actual = nib.load(output)
    assert actual.shape == native.shape
    assert actual.get_data_dtype() == np.dtype("int16")
    np.testing.assert_allclose(actual.affine, native_affine, atol=1e-5)
    grid = json.loads((constant_bundle / "grid.json").read_text())
    # Use values/geometry actually serialized to NIfTI, including affine precision.
    rigid = nib.load(header)
    task = resample_from_to(rigid, (tuple(grid["shape"]), np.array(grid["affine"])), order=1)
    coords = np.where(np.asarray(task.dataobj) != 0)
    task_labels = np.zeros(task.shape, dtype=np.int16)
    task_labels[tuple(slice(c.min(), c.max() + 1) for c in coords)] = label
    expected = resample_from_to(nib.Nifti1Image(task_labels, task.affine),
                               (native.shape, rigid.affine), order=0)
    np.testing.assert_array_equal(actual.get_fdata(), expected.get_fdata())
    assert label in actual.get_fdata()
    debug_task = nib.load(tmp_path / "labels.nii_pet_task.nii.gz")
    np.testing.assert_allclose(debug_task.get_fdata(), task.get_fdata(), atol=1e-3)
    receipt = json.loads((tmp_path / "labels.nii_receipt.json").read_text())
    assert receipt["tta"] is tta
    np.testing.assert_allclose(receipt["native_affine"], nib.load(source).affine, atol=1e-5)
