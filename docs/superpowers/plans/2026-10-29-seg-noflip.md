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

- [ ] Add failing tests: CLI has no switches and rejects --tta/--no-tta/--flip; Python/Slicer reject either legacy boolean; bundle rejects axes and enabled default; geometry test requires receipt false and a single prediction.
- [ ] Run tests in Docker against current implementation; expect the new assertions to fail.
- [ ] Remove selectors, mirror inference branch and mirrored indexing; enforce empty axes/default_tta=false. Preserve one-pass Gaussian rounding and restoration.
- [ ] Run native build and targeted tests in Docker; expect all pass. Run wrapper suite; expect no failures.
- [ ] Commit the contract change with its tests.

### Task 2: Import the model and bind new distribution versions

**Files:** localizer/src/assets/models/seg/*; localizer/src/{CMakeLists.txt,conanfile.py,README.md}; localizer/src/core/config/Version.h; python/*/pyproject.toml; python/*/scripts/vendor_dccccore.py; python/*/src/*/{__init__,runtime,segmentation}.py as applicable; python/dcccpy/README.md; .github/workflows/cross-compile.yml; scripts/package-seg-model.py; distribution tests.

**Interfaces:** Consumes no-selector API. Produces validated no-flip bundle, core 4.6.1-alpha, wrapper 0.3.0a5, runtime 0.2.0a4; downloads DCCCseg-4.6.1-alpha.zip into a version-specific verified cache.

- [ ] Import reviewed archive data and network; remove obsolete export/pilot entrypoints from shipped bundle. Adapt supplied reference functions to have no TTA parameters or CLI switch.
- [ ] Add/run RED tests that the reference API rejects legacy options and that old model bytes cannot satisfy the new cache contract.
- [ ] Update reference API, pin checksum and versions; keep all PyPI model exclusions. Include upstream file manifest in standalone ZIP with accurate integrated sizes.
- [ ] Docker: run complete native, wrapper and bundle suites; compile/install; validate actual CPU model against reference on shared index-preserving rigid header, output shape/affine/dtype and modes. No claim of independent rigid-checkpoint parity.
- [ ] Run actionlint and release metadata/ZIP checks; commit validated changes.

### Task 3: Publish and verify

**Files:** existing plan (this file), release validation evidence.

**Interfaces:** Consumes validated commit and all build assets. Produces prerelease v4.6.1-alpha, standalone model plus native/Slicer ZIPs, four runtime wheels and wrapper alpha.

- [ ] Fresh whole-branch review; address important findings with RED/GREEN tests and record decisions.
- [ ] Push dev and dispatch cross-compile.yml with release_version=4.6.1-alpha; verify dispatch commit.
- [ ] At half-hour cloud checks, inspect failures or successful five-platform release; no repeated fast polling.
- [ ] Verify core ZIP checksums/model hash and Slicer inventories. Build runtime wheels from official ZIPs in clean Docker staging; verify tags, sizes <100 MB, versions and absence of seg assets.
- [ ] Upload runtime wheels first, then wrapper wheel/sdist using mounted PyPI credentials without logging them.
- [ ] Compare published PyPI file hashes and perform a fresh public-PyPI Linux install, dependency check, public model download and native-space inference without any flip selector.
- [ ] Update this plan with URLs/results and commit/push evidence.
