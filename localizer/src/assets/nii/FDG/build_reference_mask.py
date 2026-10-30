"""Rebuild the SCAN-inspired reference (development only: numpy, nibabel).

Usage: python build_reference_mask.py /path/to/clinica-checkout
The checkout must be revision c9fb3d3b8c1b8373d9d4d7039cac378533c072a6.
"""
import hashlib
from pathlib import Path
import sys

import nibabel as nib
import numpy as np

REVISION = "c9fb3d3b8c1b8373d9d4d7039cac378533c072a6"
VERMIS_LABELS = [9100, 9110, 9120, 9130, 9140, 9150, 9160, 9170]


def resample_binary(source, foreground, target):
    """Nearest neighbor in world coordinates, with zero outside source FOV."""
    indices = np.indices(target.shape).reshape(3, -1)
    mapping = np.linalg.inv(source.affine) @ target.affine
    coordinates = np.floor(mapping[:3, :3] @ indices + mapping[:3, 3, None] + 0.5).astype(int)
    valid = np.all((coordinates >= 0) & (coordinates < np.array(source.shape)[:, None]), axis=0)
    result = np.zeros(indices.shape[1], dtype=np.uint8)
    result[valid] = foreground[tuple(coordinates[:, valid])]
    return result.reshape(target.shape)


def main():
    root = Path(sys.argv[1]) / "clinica/resources"
    destination = Path(__file__).resolve().parent
    target = nib.load(destination.parent / "paddedTemplate.nii")
    pons_path = root / "masks/region-pons_eroded-6mm_mask.nii.gz"
    atlas_path = root / "atlases/atlas-AAL2_dseg.nii.gz"
    expected_hashes = {
        pons_path: "d7dfabcaba1b48d165e7ac9427e19c66064137c5f5dddf29919a5cb7f30ffab0",
        atlas_path: "f6bc698f778a4b383abd3ce355bfd4505c4aa14708e4a7848f8ee928c2b56b37",
    }
    for path, expected in expected_hashes.items():
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError(f"Unexpected source mask: {path}; use Clinica {REVISION}")
    pons = nib.load(pons_path)
    atlas = nib.load(atlas_path)
    pons_mask = resample_binary(pons, np.asarray(pons.dataobj) > 0, target)
    vermis_mask = resample_binary(atlas, np.isin(np.asarray(atlas.dataobj), VERMIS_LABELS), target)
    union = pons_mask | vermis_mask
    image = nib.Nifti1Image(union, target.affine)
    image.set_qform(target.affine, code=1)
    image.set_sform(target.affine, code=1)
    output = destination / "FDG_Pons_Vermis_Ref.nii.gz"
    nib.save(image, output)
    for path in (pons_path, atlas_path, output):
        print(path.name, hashlib.sha256(path.read_bytes()).hexdigest())
    for name, mask in (("pons", pons_mask), ("vermis", vermis_mask), ("union", union)):
        print(name, int(mask.sum()), "voxels")


if __name__ == "__main__":
    main()
