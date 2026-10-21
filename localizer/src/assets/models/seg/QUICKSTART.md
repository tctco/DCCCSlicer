# DCCC Seg integration quick start

Complete model + scripts + controls:
`/mnt/TOSHIBA-MG09-A/dccc-eval-private-cheng/dccc_seg_onnx_20261022`

Use the repository's `.venv` (Python 3.12; nnunetv2 2.8.1). Copy the entire bundle
for handoff. The segmentation checkpoint is not needed at inference time.

```bash
python /path/to/bundle/dccc_seg.py \
  --bundle /path/to/bundle \
  --input pet_native.nii.gz \
  --rigid-header pet_after_two_rigid_iterations.nii.gz \
  --output seg_native.nii.gz \
  --work-dir /hdd/private/new_case_work \
  --mode freesurfer
```

- The rigid header must preserve the original PET voxel array and belong to the
  same PET. The script materializes the actual training grid before official
  nnU-Net preprocessing. Do not pass an already resampled aligned image here.
- `freesurfer`: default, original DKT/aseg IDs; `training`: class IDs 0–95;
  `neurosegmenter`: official regional aggregation with **no brainstem/pons split**.
  NeuroSegmenter IDs 45/46 are unavailable; pons SUVR is unsupported.
- Output NIfTI has the native PET shape and affine. Spatial restoration uses
  nearest-neighbor sampling through the recorded rigid/task grids.
- Network: FP32 `image` [1,1,128,192,128] -> `logits` [1,96,128,192,128],
  using official preprocessed axis order. Default mirror TTA is enabled.
- `labels.json` defines all reversible classes; `neurosegmenter_mapping.json`
  defines aggregated labels and limitations. `dccc_seg.py` exposes `segment()`
  and `map_labels()` for integration; see README.md for Python examples.
- Existing outputs are never overwritten. Work directory must be new, private,
  and on HDD. Inspect a representative pilot again after integration changes.

See `validation.json` for measured checks and `README.md` for full contracts.

Verified on one real FDG CPU case with default TTA: 99.99909% native voxel
agreement with PyTorch, 30 differing voxels; native shape/affine preserved.
Four regression tests pass. GPU/provider changes require a fresh pilot.
