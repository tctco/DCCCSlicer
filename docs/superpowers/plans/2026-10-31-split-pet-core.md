# Split ADNI and SCAN PET preprocessing

User chose separate interfaces: restore ADNI FDG iterative global-mean scaling;
introduce scan-pet-core with pons/vermis top-half reference normalization.
Reuse the same registration, dynamic motion correction, selected export levels,
defacing, batch and BIDS processing. SCAN currently accepts only explicit FDG;
Aβ/tau/DAT remain on ADNI with their existing reference behavior. Batch Level 3
SCAN output uses _SCAN_style.nii to distinguish it from _ADNI_style.nii.

- Add failing CLI regressions for restored ADNI fixed-point behavior and SCAN
  reference dependence; add Python forwarding and validation coverage.
- Restore the exact historical iterative implementation, retaining the new
  reference calculation separately. Route with an explicit request option.
- Register the new CLI using the shared PET-core implementation; expose
  dcccpy.scan_pet_core with the same export/deface controls.
- Update current usage docs/skill while preserving the published 4.6.2 release
  notes as historical behavior. No new release/version/tag is requested here.
- Build in Docker; run C++ reference tests, ADNI/SCAN CLI regressions, Python
  package tests, and review the final diff before reporting completion.

Verification: Docker DCCCcore build passed; standalone CTest 2/2 passed;
ADNI/SCAN CLI plus asset tests 16 passed; shared dynamic-input/help tests
27 passed (17 unrelated cases deselected); Python package tests 58 passed,
1 opt-in TestPyPI test skipped. Separate code review found no significant
issues. An intermediate CLI run overlapped linking and hit transient executable
permission errors; the sequential post-build rerun passed all 16 tests.
