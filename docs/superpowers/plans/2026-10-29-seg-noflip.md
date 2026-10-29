# Bound No-flip Segmentation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Bind the new no-flip checkpoint without exposing any inference mirroring option and release verified alphas.

**Architecture:** Single unflipped native ONNX pass; retain existing spatial preprocessing/restoration and accumulator rounding. Remove selectors at every boundary, including bundled Python reference entrypoints. Keep standalone model delivery outside PyPI.

**Tech Stack:** C++17, ITK, ONNX Runtime 1.18.1 CPU, Python, Slicer Qt, Docker, GitHub Actions, PyPI.

**Spec:** docs/superpowers/specs/2026-10-29-seg-noflip-design.md

## Global Constraints

- No TTA or flip option, not even an opt-in; reject legacy switches/keywords.
- Empty mirroring axes and disabled default TTA are required bundle metadata.
- Preserve original-space labels, int16 dtype, three vocabularies and pons limitations.
- Core/Slicer 4.6.1-alpha; dcccpy 0.3.0a5; runtime packages 0.2.0a4.
- Synchronize the three full AGENTS.md version paths; model hash `a60ad8bcd4f6c6e1fd0fc739784da392ff5d65eafeed1e6091c4608ef28db81e`.
- All compilation/tests/package checks/uploads in Docker. Preserve user untracked files.

## Review Focus

- Legacy Python calls with tta=True or False must fail before spawning native code.
- Nonempty, malformed or enabled TTA bundle declarations fail before inference.
- One prediction must preserve spatially asymmetric labels, not merely constant logits.
- New model caches must reject the old hash and never reuse the old version directory.
- Reference scripts and UI must not retain a hidden flip selector.

### Task 1: Enforce the no-flip inference contract

**Files:** localizer/src/core/segmentation/{PetSegmenter.h,PetSegmenter.cpp,SegmentationBundle.cpp,SegmentationInference.cpp}; localizer/src/spatialNormalizations/seg/SegCLI.cpp; localizer/src/tests/test_seg_cli.py; localizer/lib/pet_segmentation.py; localizer/localizer.py; localizer/Resources/UI/localizer.ui; localizer/src/tests/test_ui_segmentation.py; python/dcccpy/src/dcccpy/core.py; python/dcccpy/tests/test_core.py.

**Interfaces:** Consumes approved empty-axis manifest. Produces seg(input, output, mode, threads, bundle, rigid_header, debug, config, check, executable), with no tta keyword; Slicer command/segment_async have no tta keyword; Options has no tta member. Receipt tta is always false.

- [x] Add failing tests: CLI has no switches and rejects --tta/--no-tta/--flip; Python/Slicer reject either legacy boolean; bundle rejects axes and enabled default; geometry test requires receipt false and a single prediction.
- [x] Run tests in Docker against current implementation; expect the new assertions to fail.
- [x] Remove selectors, mirror inference branch and mirrored indexing; enforce empty axes/default_tta=false. Preserve one-pass Gaussian rounding and restoration.
- [x] Run native build and targeted tests in Docker; expect all pass. Run wrapper suite; expect no failures.
- [x] Commit the contract change with its tests.

### Task 2: Import the model and bind new distribution versions

**Files:** localizer/src/assets/models/seg/*; localizer/src/{CMakeLists.txt,conanfile.py,README.md}; localizer/src/core/config/Version.h; python/*/pyproject.toml; python/*/scripts/vendor_dccccore.py; python/*/src/*/{__init__,runtime,segmentation}.py as applicable; python/dcccpy/README.md; .github/workflows/cross-compile.yml; scripts/package-seg-model.py; distribution tests.

**Interfaces:** Consumes no-selector API. Produces validated no-flip bundle, core 4.6.1-alpha, wrapper 0.3.0a5, runtime 0.2.0a4; downloads DCCCseg-4.6.1-alpha.zip into a version-specific verified cache.

- [x] Import reviewed archive data and network; remove obsolete export/pilot entrypoints from shipped bundle. Adapt supplied reference functions to have no TTA parameters or CLI switch.
- [x] Add/run RED tests that the reference API rejects legacy options and that old model bytes cannot satisfy the new cache contract.
- [x] Update reference API, pin checksum and versions; keep all PyPI model exclusions. Include upstream file manifest in standalone ZIP with accurate integrated sizes.
- [x] Docker: run complete native, wrapper and bundle suites; compile/install; validate actual CPU model against reference on shared index-preserving rigid header, output shape/affine/dtype and modes. No claim of independent rigid-checkpoint parity.
- [x] Run actionlint and release metadata/ZIP checks; commit validated changes.

### Task 3: Publish and verify

**Files:** existing plan (this file), release validation evidence.

**Interfaces:** Consumes validated commit and all build assets. Produces prerelease v4.6.1-alpha, standalone model plus native/Slicer ZIPs, four runtime wheels and wrapper alpha.

- [x] Fresh whole-branch review; address important findings with RED/GREEN tests and record decisions.
- [x] Push dev and dispatch cross-compile.yml with release_version=4.6.1-alpha; verify dispatch commit.
- [x] At half-hour cloud checks, inspect failures or successful five-platform release; no repeated fast polling.
- [x] Verify core ZIP checksums/model hash and Slicer inventories. Build runtime wheels from official ZIPs in clean Docker staging; verify tags, sizes <100 MB, versions and absence of seg assets.
- [x] Upload runtime wheels first, then wrapper wheel/sdist using mounted PyPI credentials without logging them.
- [x] Compare published PyPI file hashes and perform a fresh public-PyPI Linux install, dependency check, public model download and native-space inference without any flip selector.
- [x] Update this plan with URLs/results and commit/push evidence.

## Local verification before cloud dispatch

- Source model SHA-256 verified: `a60ad8bcd4f6c6e1fd0fc739784da392ff5d65eafeed1e6091c4608ef28db81e`; 62,438,751 bytes, ONNX IR 8/opset 17, 100 FP16-storage initializers and float32 external I/O.
- Complete native suite: 168 passed, 8 skipped, 613.07 seconds. Added real installed-model smoke subsequently passed in native/UI suite: 37 passed, 28.52 seconds. All native production code is unchanged since the complete run.
- Whole Python/reference suite after review fix: 65 passed, 1 skipped (opt-in TestPyPI).
- Actual PET shared-rigid-geometry parity: 3,280,896 native voxels, zero differing labels, original shape `(192,192,89)` and affine preserved. C++10.28s/reference11.67s on the local CPU environment; these are observations, not cross-platform speed claims.
- Native rigid header preserves values through exactly two predictions; independent rigid checkpoint parity remains unavailable.
- Installed wheel + slim runtime + auto-provisioned model passed actual native-space int16 inference. Main wheel/sdist passed Twine checks; package does not carry ONNX files. All intended alpha versions were unused on PyPI at preparation.
- actionlint and release-metadata validation passed; all three AGENTS.md version files agree.
- Fresh whole-branch review found one Important config/bundle override bypass. Six RED→GREEN regressions now enforce the exact bound core version on every seg call. Explicit config/model resolution is retained.
- Deferred minor: the optional Python reference script may perform rigid work before rejecting an invalid custom bundle; segmentation ONNX still cannot run with invalid no-flip metadata. Native validation and main dcccpy paths are protected.
- No interactive Slicer pilot, clinical study replication or independent rigid checkpoint parity is claimed. Non-Linux execution of the actual new model is gated by the added CI smoke test.

## Cloud checkpoint

- `dev` pushed through `6a847faed5e5524e9f6c5041251bbda74e2eb9f6`.
- Dispatched 2026-10-29 10:07 UTC:
  https://github.com/tctco/DCCCSlicer/actions/runs/40855070837
- Confirmed workflow head matches the intended commit. At 10:08 UTC it was in
  progress; next ordinary cloud inspection is no earlier than 10:38 UTC.
- PyPI upload is pending successful five-platform release assets; no alpha has
  been uploaded yet. Final wrapper wheel is 16,040 bytes and contains the
  mandatory native-version guard, with no ONNX files.

### First attempt and scoped retry

At 11:09 UTC the four primary platforms succeeded. macOS 15 failed only the
unchanged motion-correction synthetic alignment test: MSE ratio ~0.491 against
the unchanged <0.35 threshold. All segmentation tests, including the actual
FP16-storage network and asymmetric no-mirroring test, passed there.

ITK 5.3 REGULAR metric sampling includes random within-voxel perturbations and
an implicit seed. Six identical-input Docker repetitions varied from MSE ratio
~0.00062 to ~0.249. This establishes stochastic variability, not a segmentation
regression. The failed job was retried once on the same unchanged source SHA;
no motion code, model code, test threshold or platform coverage was changed.
No PyPI files have been uploaded; public-release verification remains mandatory.

## Completed public release

- All five platform jobs and Create Release succeeded on attempt 2 of
  https://github.com/tctco/DCCCSlicer/actions/runs/40855070837 .
  The unchanged motion test passed on the one retry; segmentation passed on
  both macOS 15 attempts. No test threshold or coverage was weakened.
- Published prerelease:
  https://github.com/tctco/DCCCSlicer/releases/tag/v4.6.1-alpha
  at `6a847faed5e5524e9f6c5041251bbda74e2eb9f6`, published 2026-10-29 12:13 UTC.
- Independent model ZIP: 57,891,911 bytes. Actual dcccpy download from the public
  Release URL verified the bound ONNX SHA-256.
- All four primary core Actions ZIPs exactly matched public Release
  SHA256SUMS before PyPI upload; actual contained model hash was verified.
  All five Slicer ZIP inventories had the correct new 62,438,751-byte model
  and matching CRC, with obsolete pilot script absent.
- Runtime wheels `0.2.0a4` were published before wrapper `0.3.0a5`:

  | Distribution | Published bytes |
  | --- | ---: |
  | dcccpy wheel | 16,040 |
  | dcccpy sdist | 25,466 |
  | Linux x64 runtime | 93,445,042 |
  | Linux ARM64 runtime | 92,754,691 |
  | Windows x64 runtime | 87,307,625 |
  | macOS ARM64 runtime | 90,247,851 |

- Main package: https://pypi.org/project/dcccpy/0.3.0a5/ .
  Every published file size and SHA-256 matched its validated local artifact.
  No wheel includes segmentation assets; all runtime wheels are below 100 MB
  and have the correct platform tags and core version.
- A fresh isolated Docker venv installed `dcccpy[runtime]==0.3.0a5` from
  `https://pypi.org/simple`; pip check found no broken requirements.
  With core auto-download disabled and the separately downloaded public model
  cache, the installed PyPI runtime completed actual native-space segmentation
  at `(192,192,89)`, `int16`, original affine preserved, exactly one prediction.
  Both legacy boolean TTA keywords were rejected before execution.
- Release notes explicitly document the removed alpha interfaces, exact native
  version binding, model storage/arithmetic distinction, provenance limits and
  the pre-existing stochastic motion-test observation.

## Integration decisions and remaining limits

The user-authorized dev/in-place workflow and approved short design were kept;
existing user untracked files were preserved. Publication was gated on real
artifact verification rather than static review alone. Exact seg runtime
binding means SDK/native upgrades must be matched. Non-Linux actual-network
execution is now verified by CI. No interactive Slicer GUI pilot, independent
rigid-checkpoint comparison or clinical-study replication is claimed.

One reviewed minor remains deferred: the optional reference script can perform
rigid work before rejecting invalid custom model metadata, wasting time but not
allowing segmentation mirroring. Existing stochastic motion alignment variability
was not changed in this model release; it is recorded above rather than hidden.
