# FDG reference normalization implementation plan

Goal: replace FDG Level 3 iterative global scaling with a SCAN-inspired
pons/vermis top-half reference, preserving all registration and export behavior.
The user accepts an atlas approximation rather than the unavailable SCAN mask.

- Build a binary union of Clinica's eroded pons and AAL2 Vermis_1_2 through
  Vermis_10 using physical coordinates and nearest-neighbor resampling onto the
  bundled padded MNI 2 mm grid. Preserve provenance, hashes, and source notices.
- Test top-half reference statistics (even/odd counts, ties, invalid samples)
  before implementation. Use ceil(N/2), including zero-valued reference voxels;
  reject non-finite samples and non-positive resulting means.
- Compute the divisor in normalized space, exactly as existing reference-region
  tracers do, then divide the existing ADNI-grid rigid output by that scalar.
  Add an fdg_pons_vermis_ref configuration entry to both bundled configurations
  and the built-in fallback. Do not change registration or motion correction.
- Update CLI/API documentation and the historical FDG compatibility note.
  Verify numerical behavior and asset geometry; run available CLI regressions.

No version bump or Git tag is requested. No new runtime dependencies.

Validation results: full DCCCcore build passed using the existing Conan cache;
standalone CTest 2/2 passed; FDG asset test passed; ADNI CLI regression 11/11
passed; Python dcccpy suite 52 passed, 1 skipped. Reviewed the mask overlaid on
padded MNI T1 in three planes and obtained a separate read-only code review
(no important findings). Initial CLI setup lacked the assets link; rerunning
with the source assets beside the built executable passed all cases.
