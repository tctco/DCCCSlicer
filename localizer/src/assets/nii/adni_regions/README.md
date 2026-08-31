# ADNI-style regional atlas

`padded_aparc_aseg.nii.gz` is a nearest-neighbor resampling of FreeSurfer's
`fsaverage/mri/aparc+aseg.mgz` onto DCCC's padded 2 mm MNI grid. The source was
downloaded from the official FreeSurfer tutorial archive:

https://surfer.nmr.mgh.harvard.edu/ftp/data/archive/tutorial_data_Sep_2018/fsfast-tutorial.subjects/fsaverage/mri/aparc+aseg.mgz

Source SHA-256: `4c7db4478ccc171f7c891378f2974d0e140ae96131a5acd202bf7f44098f56b5`

Derived atlas SHA-256: `15b41709d0affe458559e59ce0ecec3d11399901a5d1575caf8090fe1e6ebab2`

All or portions of this licensed product (such portions are the "Software")
have been obtained under license from The General Hospital Corporation
"MGH" and are subject to the terms and conditions in
`LICENSE.FreeSurfer.txt`. This resampled volume is a modified, derived version
and is not the original FreeSurfer atlas file.

The volume uses FreeSurfer's Desikan-Killiany cortical labels together with
`aseg` subcortical labels. The ADNI-style masks use these IDs:

- Ventricles: 4, 5, 43, 44
- Hippocampus: left 17, right 53
- Entorhinal cortex: left 1006, right 2006
- Fusiform gyrus: left 1007, right 2007
- Middle temporal gyrus: left 1015, right 2015

`WholeBrain` follows a `BrainSegNotVent`-style tissue union: cortical ribbon,
cerebral and cerebellar white/gray matter, deep gray structures, brain stem,
and corpus callosum are included; ventricles, CSF, vessels, and choroid plexus
are excluded. `Ventricles` follows the ADNIMERGE convention of summing left
and right lateral and inferior-lateral ventricles rather than third/fourth
ventricles.

The intracranial mask is an atlas-warped proxy derived from DCCC's padded MNI
brain mask. It is not FreeSurfer eTIV, which is a scalar estimated from the
Talairach transform and has no corresponding `aseg` voxel mask.
