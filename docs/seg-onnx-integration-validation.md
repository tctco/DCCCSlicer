# DCCC Seg integration validation — 2026-10-22

## Scope and source

Implemented on `feat/adni-pet-deface`, based on synchronized remote commit
`1f9d933`, as requested. Software version remains `4.5.1-alpha`; no release tag,
push, or publication was performed. Existing unrelated untracked Python
packaging files were preserved.

Source bundle: desktop `dccc_seg_onnx_20261022.tar.gz`.
Installed model: `assets/models/seg/dccc_seg.onnx`, 124,818,840 bytes,
SHA-256 `8ea46dc8eff44e7a6c55b93ec9a353eb676692741d56a47b04c755a3c956faf6`.
The model uses Git LFS. Original controls, reference scripts, provenance and
validation notes accompany it.

Interfaces:

- `DCCCcore seg --input pet.nii.gz --output labels.nii.gz`.
- `dcccpy.seg(input, output=None, mode="freesurfer", threads=4, tta=True, ...)`.
- Slicer: selected input volume → **PET segmentation → Segment PET**, asynchronous
  execution and named categorical labelmap overlay.

## Docker build

All compilation and test execution took place in Docker, container
`dccc-seg-dev`, image `dccc-core-dev:latest`:
`sha256:f29553870f8b3a572cc4b8622f2749db393c24db766e2f9d175f366cc6a9b493`.
Ubuntu 22.04, GCC 11.4, CMake 3.22.1; Conan built ITK 5.3.0 and ONNX Runtime
1.18.1 for this environment. Production inference requires neither PyTorch nor
nnU-Net.

```sh
conan install . --output-folder=build-seg --build=missing \
  -s build_type=Release -s compiler.version=11 -s compiler.cppstd=gnu17 \
  -c tools.build:jobs=4 -c tools.cmake.cmaketoolchain:generator=Ninja \
  -o 'onetbb/*:tbbmalloc=False' -o 'onetbb/*:tbbproxy=False'
cmake -S . -B build-seg -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=build-seg/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/workspace/localizer/src/install-seg
cmake --build build-seg -j4
cmake --install build-seg
```

The installed Linux executable is
`localizer/src/install-seg/bin/DCCCcore`, with assets and TBB beside it. An actual
model inference invoked from `/tmp`, without `--bundle`, succeeded using the
installed default resources. `--version` reports `4.5.1-alpha`.

## Automated checks

- New native segmentation tests: **17 passed**. Covers CLI options, output
  protection, 4-D input, NaN/Inf including big-endian float64, inconsistent
  ontology/region metadata, older custom configurations, index-preserving rigid
  headers, signed output dtype, oblique native geometry, all three modes and
  default TTA using a real constant ONNX graph.
- dcccpy complete suite: **28 passed, 1 skipped**. The skipped TestPyPI test
  requires explicit opt-in. Includes argument/path handling and native failures.
- Slicer adapter plus existing UI tests: **13 passed**, with mocked Slicer/Qt
  boundaries. These are not interactive application tests.
- Original model-bundle regressions: **4 passed**.
- Complete core suite: **153 passed, 8 skipped** in 635.53 seconds. Six skipped
  FillStates accuracy cases require missing private input files; two Unicode
  regression tests are Windows-only and therefore skip in Linux Docker.

Use `DCCCCORE_TEST_EXECUTABLE=/workspace/localizer/src/install-seg/bin/DCCCcore`
to select the installed test binary without replacing an existing installation.
The reference venv is `/opt/seg-reference` inside the retained container.

## Actual model / reference comparison

Input: repository fixture `localizer/src/tests/test_batch/input.nii`, native
shape `[192,192,89]`. CPU, four threads, default eight-way mirror TTA, raw
FreeSurfer labels. The comparison script is `scripts/validate-seg-parity.py`.

| Measurement | Result |
| --- | --- |
| Native rigid predictions | Exactly 2, index-preserving intermediate volumes |
| Task PET maximum absolute error | `2.2737367544323206e-13` |
| Normalized input maximum absolute error | `2.86102294921875e-6` |
| Native voxels | 3,280,896 |
| Different native labels | **0** |
| Native label agreement | **100%** |
| Foreground union voxels | 165,149 |
| Foreground union label agreement | **100%** |
| Native shape/affine | Preserved to NIfTI serialization precision |
| C++ elapsed time | 55.09 seconds |
| Python reference elapsed time | 55.10 seconds |

Timing is one run under concurrent regression-test load, not a benchmark.
Reference environment: nnunetv2 2.8.1, torch 2.14.1+cpu, ONNX Runtime 1.23.2,
NumPy 2.2.6, SciPy 1.15.3, SimpleITK 2.5.6, nibabel 5.3.2. This differs from the
bundle author's environment; agreement above is measured for this environment.

This is **downstream segmentation parity conditioned on the C++ rigid header**.
The original rigid PyTorch checkpoint and its pipeline are not shipped, so
independent rigid-transform parity has not been established. The script checks
two native predictions and native-index preservation, then passes the exact
recorded affine through NIfTI-2 to the reference. It does not pass a rounded
NIfTI-1 header while comparing against full-precision C++ geometry.

The first comparison using NIfTI-1 transport exposed this distinction:
normalized maximum error was `1.2097e-4`, although final labels were still
identical. A diagnostic using the exact affine reduced task error to numerical
roundoff. The corrected complete comparison produced the results above.

QC images, transform receipts, arrays and machine-readable `parity.json` are
retained locally under
`.superpowers/sdd/2026-10-22-dccc-seg/parity-tta-exact/`, not committed.

## Review findings and limits

An independent read-only reviewer checked the implementation. Metadata
cross-checks, older-configuration fallback and shared Windows NIfTI path staging
were corrected. Runtime tests also exposed that niftilib repairs nonfinite
floating-point voxels to zero; strict segmentation input reads now inspect the
raw payload with niftilib's own header/compression/endianness APIs first.

Outstanding limits:

- No interactive Slicer/Qt test or visual overlay inspection was available.
  Transform hardening and scene-close cancellation require application testing.
- No Windows/macOS build was executed. Windows Unicode handling reuses existing
  staging support; the platform-specific code path was not run on Windows.
- No independent original-checkpoint rigid comparison or representative
  multi-case clinical pilot was possible from the shipped materials.
- A failed final output write may leave a partial new target. Existing targets
  are protected; use a new output path after such a failure. Atomic publication
  was a reviewer minor finding deferred from this integration.
- NeuroSegmenter IDs 45/46 remain unavailable; pons SUVR is unsupported.

These checks validate the tested integration and geometry, not clinical
segmentation accuracy or a cohort evaluation.

## Execution decisions

The user explicitly authorized implementation, so the approved design was
executed inline without another plan-approval pause. Work remained in the
requested branch checkout, preserving unrelated files. Independent wrapper/UI
work proceeded while Docker built native dependencies. The numerical oracle
uses the exact recorded affine to avoid testing different transforms. QC
artifacts are retained rather than deleting the execution workspace, so the
reported measurements remain inspectable.
