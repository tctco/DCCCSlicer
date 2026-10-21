# DCCC Seg ONNX integration handoff

Model: Dataset618_DCCC_FineDKT_1p5mm_Fold0, fold 0, final checkpoint,
96 mutually exclusive classes (background plus 95 foreground structures).
This is the trained segmentation network, separate from DCCC atlas propagation.

## Files

- `dccc_seg.onnx`: FP32 network, opset 17, approximately 120 MB.
- `dccc_seg.py`: single-volume CLI and importable `segment()` API.
- `plans.json`, `dataset.json`: official nnU-Net preprocessing controls.
- `grid.json`: actual trained PET grid, 128 x 171 x 128 at 1.5 mm.
- `labels.json`: lossless training-class to FreeSurfer/DKT label mapping.
- `neurosegmenter_mapping.json`: official regional aggregation and explicit limitations.
- `manifest.json`, `validation.json`: model contract, provenance, network checks.
- `export_onnx.py`: reproducible export from the final training folder.
- `validate_pilot.py`: optional private real-input PyTorch/ONNX parity and QC script.
- `test_bundle.py`: categorical mapping and native geometry regression tests.

The complete release bundle is stored on HDD at:
`/mnt/TOSHIBA-MG09-A/dccc-eval-private-cheng/dccc_seg_onnx_20261022`.
Copy the whole directory when handing this to another agent. Code sources also
live in `projects/dccc_seg/onnx_bundle/`. No training checkpoint is needed for
ONNX runtime inference. The optional rigid-checkpoint route requires the DCCC
repository and its existing rigid runtime dependencies.

## Environment

Reference environment: Python 3.12, torch 2.14.0, nnunetv2 2.8.1,
dynamic-network-architectures 0.4.4, onnxruntime 1.29.0, numpy 2.3.1,
nibabel 5.3.2, SimpleITK 2.5.2, scipy 1.16.0. Export also uses onnx 1.22.0.
The host repository uses `uv` and already provides these packages in `.venv`.
Use its lockfile/environment for reproducibility. PyTorch remains a runtime
dependency for official nnU-Net tiling and preprocessing; the segmentation
network executes in ONNX Runtime. CPU is the verified provider. An installed
CUDAExecutionProvider can be selected explicitly but has not been validated.

## Invocation

Recommended integration: let dcccpy/DCCCSlicer perform the established **two
iterative-rigid iterations**, then pass the native PET and its index-preserving
rigid-header image. A rigid-header image must retain the exact native voxel
array; its affine records the mapping into the aligned frame. Do not pass a
resampled aligned image under this argument.

```bash
.venv/bin/python /path/to/bundle/dccc_seg.py \
  --bundle /path/to/bundle \
  --input /path/to/pet_native.nii.gz \
  --rigid-header /path/to/pet_rigid_header.nii.gz \
  --output /path/to/seg_native.nii.gz \
  --work-dir /path/on/hdd/private_new_case_dir \
  --mode freesurfer
```

The work directory must not already exist and is created with private
permissions. It retains actual model input, task-grid training IDs, and
`receipt.json` with the exact affine chain for QC/debugging. Keep these medical
image artifacts private. All bulky work directories belong on HDD. The output
parent directory must already exist. Existing output files are never overwritten.

Alternatively, invoke the repository's existing rigid module directly:

```bash
.venv/bin/python /path/to/bundle/dccc_seg.py \
  --bundle /path/to/bundle --input /path/to/pet_native.nii.gz \
  --rigid-checkpoint /path/to/rigid.ckpt --repo /path/to/dccc \
  --output /path/to/seg_native.nii.gz \
  --work-dir /path/on/hdd/private_new_case_dir
```

This route uses CPU, exactly two rigid iterations, and no convergence-based
early stopping. Do not apply rigid normalization twice. The wrapper's
rigid-checkpoint route is provided for convenience; the real-volume ONNX pilot
uses a previously validated rigid-header result.

Python API (place bundle on `sys.path`):

```python
from pathlib import Path
from dccc_seg import segment

segment(
    input_path=Path('pet_native.nii.gz'),
    output_path=Path('seg_native.nii.gz'),
    bundle=Path('/path/to/bundle'),
    work_dir=Path('/path/on/hdd/new_private_work'),
    rigid_header=Path('pet_rigid_header.nii.gz'),
    mode='freesurfer',
)
```

Defaults: official mirror TTA, Gaussian sliding windows, step size 0.5,
CPUExecutionProvider, four threads. `--no-tta` is a faster, explicitly different
inference protocol. Neither output vocabulary nor space depends on TTA.
Do not silently change this default in an integration. Both no-TTA and default
full mirror TTA passed a real-volume CPU parity pilot; GPU execution still needs
its own pilot before a cohort launch. This handoff does not launch a cohort evaluation.

## Network and geometry contract

1. Preserve native PET values and record its native affine `A_native`.
2. Run two iterative-rigid steps, recording index-preserving `A_rigid`.
3. Materialize PET values with `A_rigid` on **the exact `grid.json` grid**, with
   linear interpolation. Updating a header alone is insufficient network input.
4. Let the official nnU-Net reader/preprocessor perform axis handling, cropping,
   normalization and task resampling. It returns C,D,H,W tensors; do not feed
   unnormalized NIfTI X,Y,Z arrays directly to ONNX.
5. ONNX input `image`: float32 `[1,1,128,192,128]` inference patches.
   Output `logits`: float32 `[1,96,128,192,128]`, before softmax/argmax.
   Graph spatial axes are dynamic for export validation; production tiling must
   use the prescribed patch. Batch and channel counts are fixed. Network probes
   use spatial multiples of 32 with more than one bottleneck spatial element.
6. Official nnU-Net Gaussian tiling and export restore training IDs onto the
   exact task grid. Keep probabilities/logits categorical until this official
   restoration is finished; do not resample integer IDs with linear interpolation.
7. Nearest-neighbor sample the task labels at `(native_shape, A_rigid)`, then
   attach `A_native`. Thus the final shape/affine and voxel indexing match the
   native PET. Do not sample task-space labels directly with `A_native`.
8. Apply the requested categorical LUT to obtain final output IDs.

For explicit transform integration, aligned-world coordinates are
`A_rigid @ inverse(A_native) @ native_world`. To put a task image into native
world coordinates, its affine is
`A_native @ inverse(A_rigid) @ A_task`. This is equivalent to step 7.
The script records both affines and the task grid in its receipt.

## Label modes

- `freesurfer` (default): reversible raw DKT/aseg IDs (e.g. 17/53 hippocampi,
  1028/2028 superior frontal cortex). `labels.json` defines every class.
- `training`: contiguous IDs 0–95 exactly as trained. These IDs must never be
  interpreted as FreeSurfer IDs.
- `neurosegmenter`: aggregate using the checked-in FDG-NeuroSegmenter official
  `label_correspondence.csv`; output IDs 1–44 and 47–52 have its regional meanings.
  This is **partial label compatibility**, not the NeuroSegmenter model or its
  preprocessing protocol. White matter, ventricles, CSF, and unmapped structures
  become background 0, so retain the default raw output if those regions matter.

**Brainstem limitation:** DCCC raw ID 16 contains the whole brainstem. It cannot
recover a separate pons. Although the upstream CSV assigns 16 to label 45
(brainstem without pons), this handoff deliberately leaves 16 as 0 rather than
claim an anatomically false split. IDs 45 and 46 are unavailable. In particular,
NeuroSegmenter downstream pons-based SUVR normalization is unsupported. The
mapping JSON marks availability for all 52 regions. Do not treat absent pons as
a measured zero volume or silently run the quantifier on it.

For an already restored training-ID array:

```python
from dccc_seg import read, map_labels
raw = map_labels(training_ids, read(bundle / 'labels.json'), 'freesurfer')
neuro = map_labels(training_ids, read(bundle / 'labels.json'), 'neurosegmenter',
                   read(bundle / 'neurosegmenter_mapping.json'))
```

## Validation and integration acceptance

`validation.json` records numerical ONNX/PyTorch network probe errors, not
clinical segmentation performance. FP32 backend differences can change a few
near-tied argmax voxels; keep the measured errors and voxel differences visible.
Private real-input results and visual QC are stored separately in the adjacent
`dccc_seg_onnx_pilot_20261022` and `dccc_seg_onnx_pilot_tta_20261022` directories,
not shipped with patient images. Both pilots were visually reviewed: actual
normalized input, materialized task PET and native-space segmentation overlays
showed no evident geometric mismatch. This establishes integration correctness
for the tested case, not clinical segmentation accuracy.

For the FDG CPU pilot, default mirror TTA yielded 99.99909% native voxel
agreement with PyTorch (30 differing voxels; 99.98424% agreement within the
foreground union). No-TTA yielded 99.99887% (37 differing voxels). Shape and
affine matched the native PET for both. These are measured differences, not
bitwise equivalence.

Before production integration, run a small representative pilot, inspect the
actual preprocessed arrays, task PET and native overlay, and verify NIfTI/Slicer
axis conventions. Repeat this gate after changing preprocessing, provider,
precision, TTA, spatial transforms or the output adapter. The Slicer integrator
must account for RAS/LPS at its image API boundary; this script uses NIfTI
physical affines and delegates nnU-Net I/O to its official reader.

Run portable regression checks with:

```bash
python -m pytest /path/to/bundle/test_bundle.py -q
```
