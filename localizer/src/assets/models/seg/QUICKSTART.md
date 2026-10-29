# DCCC no-flip nnU-Net CPU integration bundle

Selected model: FP16; final1000 fold0; 96 mutually exclusive classes.

FP16 means binary16 weight storage with FP32 network computation. It reduces the network file size; no runtime speed or memory improvement is claimed. The full held-out internal633 study used GPU with FP32 arithmetic in both arms and changed only weight rounding. The actual CPU deployment artifacts additionally passed FDG/CFT/APN input, geometry and native-output checks. Small precision/environment differences are retained in validation.json. This is a two-arm FP32/FP16 comparison, not an additional ONNX study.

## Run

Use the existing DCCC Python environment with nnunetv2, torch, onnxruntime, nibabel, numpy and SimpleITK. This bundle uses CPUExecutionProvider and no inference mirroring by default and requires no segmentation training checkpoint.

```bash
python /path/to/bundle/dccc_seg.py --bundle /path/to/bundle \
  --input /path/to/native_pet.nii.gz \
  --rigid-header /path/to/its_index_preserving_two_iteration_rigid_pet.nii.gz \
  --output /path/to/new_native_labels.nii.gz \
  --work-dir /hdd/private/new_work_directory --mode freesurfer
```

The native PET and rigid-header PET must have identical voxel arrays. The rigid header must belong to this input and record the accepted two-iteration DCCC rigid mapping. The wrapper materializes the verified 1.5mm training lattice before official nnU-Net preprocessing, then returns categorical labels to native PET space by nearest-neighbor interpolation through the recorded grids. Do not pass an already resampled PET as a rigid header and do not apply rigid normalization twice. Existing outputs are refused. Store private work assets on HDD.

The importable segment() API has the same arguments; tta=False is the default for this no-flip checkpoint. Training class IDs0-95 are mapped losslessly to the original FreeSurfer/DKT IDs by default. Optional NeuroSegmenter aggregation is anatomically partial: whole brainstem is not split into pons, labels45/46 are unavailable and pons SUVR normalization is unsupported.

The ONNX graph contains the segmentation network only, with float32 external image/logits I/O. Plans, training grid and label controls are included. CPU/GPU changes require a representative visual pilot. No patient images or patient source paths are included.
