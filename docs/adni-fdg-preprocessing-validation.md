# FDG preprocessing compatibility note

## Current default

`adni-pet-core --tracer fdg` now uses a SCAN-inspired intensity reference:
Clinica eroded pons plus AAL2 cerebellar vermis. The normalization divisor is
the mean of the highest ceil(N/2) voxel intensities within their binary union,
measured on spatially normalized PET. The existing rigid-aligned ADNI-grid
output is divided by that scalar. Motion correction, rigid registration,
and export levels are unchanged.

This is an atlas approximation, not the unavailable Berkeley hand-drawn mask.
It does not reproduce SCAN's full spatial processing or establish numerical
agreement with published SCAN SUVRs. See the [mask provenance and rebuild
instructions](../localizer/src/assets/nii/FDG/README.md).

Reference: [UC Berkeley SCAN FDG methods, May 2024](https://files.alz.washington.edu/scan/UCBerkeley_SCAN_FDG_Methods_20240529.pdf).

## Historical validation of the previous global-mean default

The results below describe the former algorithm only; they do not validate
the current pons/vermis reference normalization.

DCCCcore 4.4 adds tracer-aware `adni-pet-core` preprocessing. The command now
requires `--tracer abeta`, `--tracer tau`, `--tracer fdg`, or `--tracer dat`.
FDG uses the
iterative global-mean procedure described by the ADNI PET Core: scale the whole
image to mean 1, exclude voxels below 0.5, rescale retained voxels to mean 1,
and repeat until the excluded voxel count is unchanged.

## Validation against ADNI PET Core examples

Four original six-frame FDG studies were reconstructed as 4D NIfTI images and
processed through the complete DCCC pipeline. Each result was compared with its
date-matched ADNI `Coreg, Avg, Standardized Image and Voxel Size` DICOM series.

| Measurement | Observed range |
|---|---:|
| Voxelwise Pearson correlation | 0.783–0.913 |
| Dice coefficient for voxels >= 0.5 | 0.847–0.918 |
| DCCC final masked mean | 1.000000 |
| ADNI example final masked mean | 1.041–1.058 |
| Difference in whole-image mean | 4.0%–5.2% |

The spatial distributions are similar but the files are not expected to be
voxel-identical. DCCC and ADNI use different motion-correction and rigid
alignment implementations. More importantly, the supplied ADNI DICOM examples
are not exact fixed points of the published iterative rule: applying the rule
again requires an additional divisor of 1.057–1.074. After that additional
iteration, their whole-image means differ from the corresponding DCCC results
by only 1.7%–2.9%.

Users should therefore expect small systematic intensity differences when
comparing DCCC output directly with downloaded ADNI preprocessed DICOMs. DCCC
intentionally follows the published stopping condition and guarantees, up to
floating-point precision, that the final voxels retained at the 0.5 threshold
have mean 1. This initial scaling is not a substitute for a study-specific
reference-region normalization used by downstream analyses.

References:

- [The ADNI PET Core (Jagust et al., 2015)](https://pmc.ncbi.nlm.nih.gov/articles/PMC4510459/)
- [ADNI PET technical workflow](https://adni.loni.usc.edu/wp-content/themes/freshnews-dev-v2/documents/clinical/ADNI_Go_Protocol.pdf)
