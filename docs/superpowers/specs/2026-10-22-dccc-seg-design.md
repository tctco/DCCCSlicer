# DCCC Seg ONNX integration

## Agreed scope and baseline

Integrate the desktop bundle `dccc_seg_onnx_20261022.tar.gz` into DCCCcore,
with corresponding DCCCSlicer and dcccpy interfaces. The user approved native
C++/ONNX Runtime inference and requested the command name `seg`.

Use `feat/adni-pet-deface`, synchronized with
`origin/feat/adni-pet-deface` at `1f9d933`, as explicitly requested by the user.
Its software version is `4.5.1-alpha`. GitHub was fetched before work.
Keep that version until a release increment is requested. All compilation and
test execution must occur in Docker. Preserve existing untracked Python
packaging files. Do not publish, push, or create a release as part of integration.

## Architecture

Implement an independent C++ segmentation service under
`localizer/src/core/segmentation/`, with separate responsibilities for bundle
metadata, spatial preprocessing/restoration, and ONNX patch inference.
Register a CLI adapter under `localizer/src/spatialNormalizations/seg/` through
`localizer/src/spatialNormalizations/ModuleCatalog.cpp` and
`localizer/src/CMakeLists.txt`.

Use existing ITK, Eigen, and ONNX Runtime dependencies. Reproduce the bundle's
nnU-Net preprocessing and prediction behavior in C++; PyTorch and nnU-Net are
reference-test dependencies inside Docker, not production runtime requirements.
Select an explicit JSON parser dependency if existing dependencies do not expose
a suitable supported parser. Do not implement ad hoc JSON text extraction.

Install `dccc_seg.onnx`, `grid.json`, `plans.json`, `dataset.json`, `labels.json`,
`neurosegmenter_mapping.json`, `manifest.json`, and `validation.json` under
`localizer/src/assets/models/seg/`. Preserve upstream provenance and reference
code for parity validation. Track the model using the repository's ONNX Git LFS
rule. Existing asset installation must include these files in runtime bundles.

## CLI contract

```sh
DCCCcore seg --input pet.nii.gz --output labels.nii.gz \
  --mode freesurfer --threads 4
```

Options:

- `--mode`: `freesurfer` (default), `training`, or `neurosegmenter`.
- `--threads`: positive integer, default 4, for CPU inference.
- `--no-tta`: explicitly disable the default mirror augmentation.
- `--bundle`: optional override of the installed model bundle directory.
- `--rigid-header`: optional externally prepared index-preserving rigid result
  for reference parity and advanced integration. Verify dimensions and voxel
  values against the input before use.
- `--config`: existing configuration override for rigid resources.
- `--debug`: save intermediate geometry and preprocessing artifacts using the
  existing debug-output conventions, including transform and inference settings.

Accept one finite 3-D PET volume with positive signal. Reject 4-D images, empty
signal, invalid geometry, unknown modes, invalid threads, missing or inconsistent
bundle metadata, and existing output targets with an actionable nonzero failure.
Reject unsupported batch/BIDS options explicitly if exposed by base arguments.
Do not silently skip rigid alignment or fall back to atlas propagation.

Write a signed 16-bit categorical NIfTI label image with the native PET shape,
spacing, origin, and direction. Preserve native spatial metadata to NIfTI
serialization precision. The reference script remains the acceptance oracle.

## Spatial and numerical contract

1. Record native geometry and preserve the native PET voxel values.
2. Unless a validated `--rigid-header` is supplied, perform exactly two rigid
   model predictions with no convergence-based early stop. Check the existing
   `alignIterative` counting: it performs one initial prediction plus `maxIter`
   additional predictions. Avoid accidentally performing three or six steps.
   Validate the native rigid implementation against the reference protocol.
3. Materialize the actual training grid with linear interpolation: shape
   `[128,171,128]`, spacing 1.5 mm, NIfTI RAS affine
   `[[-1.5,0,0,94.25],[0,1.5,0,-144.5],[0,0,1.5,-86.25],[0,0,0,1]]`.
   Convert RAS/LPS deliberately at the ITK boundary. A header update alone is
   insufficient network input.
4. Match official SimpleITK reader axis order, nonzero cropping/fill behavior,
   and unmasked Z-score normalization from `plans.json`. The fixed task grid and
   requested target spacing are both 1.5 mm; validate any crop and padding before
   deciding whether a resampling operation can be omitted.
5. Run FP32 input `image` with patches `[1,1,128,192,128]`; expect FP32 `logits`
   `[1,96,128,192,128]`. Check names, types, shapes, and finite outputs.
6. Match official symmetric padding, sliding-window steps of 0.5, Gaussian
   weighting, and mirror combinations over axes `[0,1,2]`. Default TTA is enabled.
   Match reference accumulation precision and categorical decisions; investigate
   deviations instead of inventing a permissive agreement threshold.
7. Restore logits/categorical results through official crop and task-grid rules.
   Native restoration samples the task labels using the rigid affine with
   nearest-neighbor interpolation, then attaches the original native geometry.
   Never interpolate integer labels linearly or restore directly using the
   unaligned native affine.
8. Apply a checked categorical lookup table for the chosen label mode.

Release acceptance includes parity evidence for actual normalized arrays,
task geometry, default-TTA predictions, and final native labels. Record measured
differences and test limitations. A successful build alone is insufficient.

## Label vocabulary

`training` emits IDs 0–95. `freesurfer` emits the reversible raw DKT/aseg IDs
defined in `labels.json`. `neurosegmenter` uses only the supplied aggregation,
with unavailable IDs 45/46 and no brainstem/pons split. Display that limitation
in CLI help, Python documentation, and Slicer mode selection. Do not invent
pons labels or offer pons-based SUVR through this feature.

## dcccpy interface

Add and export `dcccpy.seg(input, output=None, *, mode='freesurfer', threads=4,
tta=True, bundle=None, rigid_header=None, debug=False, config=None,
check=True, executable=None)` returning the existing `DCCCResult` type.
Follow existing file/image input and temporary-output conventions. Validate
enumerated options and thread counts, forward paths as separate subprocess
arguments, and preserve failures through the established result handling.
Document the native label-space contract, TTA default, and an example.

## DCCCSlicer interface

Add a segmentation logic adapter in `localizer/lib/` and a visible PET
segmentation section in `localizer/Resources/UI/localizer.ui`, wired from
`localizer/localizer.py`. It uses the selected input volume, exposes label mode
and TTA (enabled by default), and invokes the bundled `DCCCcore seg`.
Use a unique temporary directory for each run and existing execution/error
display conventions. Load the result as a labelmap/segmentation with named
labels from the selected vocabulary and display it over the original volume.
Never load the labels as continuous PET intensity data. Retain source volume
geometry and account for any parent transform during volume export/import.

Do not install PyTorch or nnU-Net in Slicer's Python runtime. Report missing
bundled models/executables clearly. Cover command construction, mode names,
default TTA, failure propagation, and labelmap loading with mocked Slicer tests.
If a real Slicer application is unavailable, state that interactive visual
acceptance is outstanding rather than claiming it was tested.

## Docker verification

Restore access to Docker Desktop's Linux engine without deleting containers,
images, volumes, or resetting Docker. Existing Windows CLI is at
`C:\Program Files\Docker\Docker\resources\bin\docker.exe`; both known Linux
engine pipe endpoints were unavailable during initial inspection.

Use the repository Docker build environment and Conan/CMake setup. Keep
reference Python dependencies in an isolated container environment. Run:

- Existing baseline tests relevant to spatial commands and Python wrappers.
- New CLI validation and model-contract tests.
- Geometry/categorical restoration tests with asymmetric and oblique fixtures.
- Bundle's portable regression tests and preprocessing/prediction parity checks
  using the supplied Python reference, including default TTA.
- dcccpy command/result tests and Slicer adapter tests.
- A Docker Release build and installed-tree `seg` smoke/inference test, ensuring
  model/control assets resolve outside the source/build working directory.

Use available repository fixtures initially. For a representative real-input
pilot, use only a suitable identified input placed in scope; report any absence
of a real patient pilot and visual QC explicitly. Build or dependency failures
must remain visible in the handoff. No host compilation or host test execution.

## Review status

The user approved the architecture in chat, renamed the command to `seg`, and
selected the `4.5.1-alpha` branch. This written design is prepared for review
before the implementation plan, as required by the brainstorming workflow.
