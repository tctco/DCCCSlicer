# FDG SCAN-inspired pons/vermis reference

`FDG_Pons_Vermis_Ref.nii.gz` is a binary union of:

- Clinica `resources/masks/region-pons_eroded-6mm_mask.nii.gz` (the `pons`
  reference, 6 mm erosion; not the alternative `pons2`).
- Clinica `resources/atlases/atlas-AAL2_dseg.nii.gz`, labels 9100, 9110,
  9120, 9130, 9140, 9150, 9160, 9170 (`Vermis_1_2` through `Vermis_10`).

Source: https://github.com/aramis-lab/clinica/tree/c9fb3d3b8c1b8373d9d4d7039cac378533c072a6/clinica/resources

Both components are independently nearest-neighbor resampled using their
NIfTI affines onto `../paddedTemplate.nii` before taking the union. The output
has shape 96 x 128 x 96, spacing 2 mm, RAS affine diagonal (-2, 2, 2) and
translation (94, -144, -86). There are 255 pons voxels and 2040 vermis voxels,
2295 union voxels (18,360 mm³). No further dilation or erosion is applied.

Rebuild with Python, NumPy, and NiBabel:

```sh
git clone https://github.com/aramis-lab/clinica.git /tmp/clinica
git -C /tmp/clinica checkout c9fb3d3b8c1b8373d9d4d7039cac378533c072a6
python build_reference_mask.py /tmp/clinica
```

SHA-256:

| File | Hash |
| --- | --- |
| Source pons | `d7dfabcaba1b48d165e7ac9427e19c66064137c5f5dddf29919a5cb7f30ffab0` |
| Source AAL2 | `f6bc698f778a4b383abd3ce355bfd4505c4aa14708e4a7848f8ee928c2b56b37` |
| Derived mask | `d6863fdd73c81b07e15aa4451e89c65ec2c233a640768b216aa794791305f860` |

Source copyright, atlas attribution and licensing notices are retained in
`LICENSE.Clinica.txt`. In particular cite Tzourio-Mazoyer et al. (2002),
Rolls, Joliot & Tzourio-Mazoyer (2015), and the WFU PickAtlas references
listed there for the pons mask. This is a modified derived mask.

The FDG divisor is the mean of the highest ceil(N/2) PET intensities in this
combined ROI, including zeros when ranking. Non-finite reference samples,
empty masks, and non-positive reference means are errors. The reference
is sampled in spatially normalized PET; the divisor scales the rigid-aligned
ADNI-grid output. Consequently the mask should not be overlaid directly on
the rigid-only output to check that its reference mean equals one.

This approximates the reference-region idea in the [SCAN FDG methods](https://files.alz.washington.edu/scan/UCBerkeley_SCAN_FDG_Methods_20240529.pdf),
which use a hand-drawn MNI pons/vermis mask and its top 50% voxels. Neither
mask equivalence nor full SCAN pipeline equivalence is claimed. Anatomical
and quantitative validation on study data is still needed.

## Verification

The numerical tests use the production statistics and ITK ImageOps code:

```sh
cmake -S localizer/src/tests/fdg_reference -B /tmp/fdg-tests \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/ITK-or-conan-generators
cmake --build /tmp/fdg-tests
ctest --test-dir /tmp/fdg-tests --output-on-failure
pytest localizer/src/tests/test_fdg_reference_asset.py
```

The CLI regression is
`test_adad_cli.py::TestAdniPetCoreCLI::test_fdg_uses_pons_vermis_reference_normalization`.
It verifies finite output and that overriding the reference changes only one
intensity scale factor. Registration/inference assets must be available next
to the test executable as in a normal installation.
