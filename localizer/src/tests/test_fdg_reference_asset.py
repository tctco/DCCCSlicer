"""Geometry/provenance regression for the bundled MNI FDG reference."""
from pathlib import Path
import hashlib

import numpy as np
import pytest

nib = pytest.importorskip("nibabel")


def test_fdg_reference_matches_padded_mni_grid():
    assets = Path(__file__).resolve().parents[1] / "assets" / "nii"
    reference = nib.load(assets / "FDG" / "FDG_Pons_Vermis_Ref.nii.gz")
    template = nib.load(assets / "paddedTemplate.nii")
    assert reference.shape == template.shape
    np.testing.assert_allclose(reference.affine, template.affine, atol=1e-6)
    data = np.asarray(reference.dataobj)
    np.testing.assert_array_equal(np.unique(data), [0, 1])
    assert np.count_nonzero(data) == 2295
    world = nib.affines.apply_affine(reference.affine, np.argwhere(data > 0))
    # The union must stay in posterior midline cerebellum / brainstem,
    # detecting axis inversions or a voxel-index-only atlas combination.
    assert np.all(np.abs(world[:, 0]) <= 16)
    assert np.all((world[:, 1] >= -90) & (world[:, 1] <= -10))
    assert np.all((world[:, 2] >= -50) & (world[:, 2] <= 10))
    assert hashlib.sha256((assets / "FDG" / "FDG_Pons_Vermis_Ref.nii.gz").read_bytes()).hexdigest() == (
        "d6863fdd73c81b07e15aa4451e89c65ec2c233a640768b216aa794791305f860"
    )
