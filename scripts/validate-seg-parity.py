#!/usr/bin/env python3
"""Docker downstream-segmentation parity, conditioned on the C++ rigid header.

This does not independently validate rigid transforms against the unshipped
PyTorch checkpoint/pipeline. It verifies the two native rigid predictions and
index preservation, then compares segmentation with a shared rigid geometry.
"""
import argparse
import importlib.util
import json
from pathlib import Path
import subprocess
import time

import nibabel as nib
import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core", required=True, type=Path)
    parser.add_argument("--bundle", required=True, type=Path)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    parser.add_argument("--no-tta", action="store_true")
    parser.add_argument("--threads", type=int, default=4)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=False, mode=0o700)
    native_output = args.work_dir / "cpp.nii.gz"
    command = [str(args.core.resolve()), "seg", "--input", str(args.input.resolve()),
               "--output", str(native_output.resolve()), "--bundle", str(args.bundle.resolve()),
               "--threads", str(args.threads), "--debug"]
    if args.no_tta:
        command.append("--no-tta")
    start = time.monotonic()
    subprocess.run(command, check=True)
    cpp_seconds = time.monotonic() - start
    rigid_steps = sorted(args.work_dir.glob("cpp.nii_alignment_rigid[0-9]*.nii"))
    assert [p.name for p in rigid_steps] == ["cpp.nii_alignment_rigid0.nii", "cpp.nii_alignment_rigid1.nii"]
    original = nib.load(args.input)
    for path in rigid_steps:
        step = nib.load(path)
        assert step.shape == original.shape
        np.testing.assert_allclose(step.get_fdata(), original.get_fdata(), rtol=1e-6, atol=1e-6)
    spec = importlib.util.spec_from_file_location("dccc_seg_reference", args.bundle / "dccc_seg.py")
    reference = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reference)
    # NIfTI-1 debug headers round the rigid affine to FP32. Transport the exact
    # recorded transform in NIfTI-2 so this tests the same geometry on both sides.
    receipt = json.loads((args.work_dir / "cpp.nii_receipt.json").read_text())
    reference_header = args.work_dir / "reference_rigid_header.nii.gz"
    nib.save(nib.Nifti2Image(np.asarray(original.dataobj, dtype=np.float32),
                           np.array(receipt["rigid_affine"])), reference_header)
    start = time.monotonic()
    reference.segment(args.input, args.work_dir / "reference.nii.gz", args.bundle,
                      args.work_dir / "reference_work", rigid_header=reference_header,
                      threads=args.threads, tta=not args.no_tta)
    reference_seconds = time.monotonic() - start
    native = nib.load(args.input)
    actual = nib.load(native_output)
    expected = nib.load(args.work_dir / "reference.nii.gz")
    np.testing.assert_allclose(actual.affine, native.affine, atol=1e-4)
    assert actual.shape == native.shape == expected.shape
    assert actual.get_data_dtype() == np.dtype("int16")
    cpp_task = nib.load(args.work_dir / "cpp.nii_pet_task.nii.gz")
    reference_task = nib.load(args.work_dir / "reference_work/pet_task_0000.nii.gz")
    np.testing.assert_allclose(cpp_task.affine, reference_task.affine, atol=1e-4)
    cpp_data = np.asarray(nib.load(args.work_dir / "cpp.nii_preprocessed.nii.gz").dataobj).transpose(2, 1, 0)
    reference_data = np.load(args.work_dir / "reference_work/preprocessed_input.npy")[0]
    np.testing.assert_allclose(cpp_data, reference_data, atol=1e-5, rtol=1e-5)
    a, b = np.asarray(actual.dataobj), np.asarray(expected.dataobj)
    union = (a != 0) | (b != 0)
    import onnxruntime
    import torch
    report = {"input": str(args.input), "tta": not args.no_tta, "threads": args.threads,
              "parity_scope": "downstream segmentation conditioned on the C++ rigid header",
              "native_rigid_predictions": len(rigid_steps),
              "rigid_header_transport": "NIfTI-2 carrying the exact JSON affine",
              "independent_rigid_checkpoint_parity": "not available: reference checkpoint/pipeline not shipped",
              "cpp_seconds": cpp_seconds, "reference_seconds": reference_seconds,
              "reference_ort": onnxruntime.__version__, "reference_torch": torch.__version__,
              "native_shape": list(native.shape), "native_affine_preserved": True,
              "task_pet_max_abs_error": float(np.max(np.abs(cpp_task.get_fdata() - reference_task.get_fdata()))),
              "normalized_input_max_abs_error": float(np.max(np.abs(cpp_data - reference_data))),
              "native_voxels": int(a.size), "different_voxels": int(np.count_nonzero(a != b)),
              "native_agreement": float(np.mean(a == b)),
              "foreground_union_voxels": int(union.sum()),
              "foreground_union_agreement": float(np.mean(a[union] == b[union])) if union.any() else None}
    (args.work_dir / "parity.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
