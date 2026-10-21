"""Contract tests for label vocabularies and reversible native geometry."""
import importlib.util
from pathlib import Path
import nibabel as nib
import numpy as np
import pytest

spec = importlib.util.spec_from_file_location('dccc_seg', Path(__file__).with_name('dccc_seg.py'))
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


def test_label_mapping_and_brainstem_limitation():
    ontology = {'raw_ids': [0, 16, 17, 53, 1028, 2028]}
    ids = np.arange(6).reshape(1, 2, 3)
    raw = m.map_labels(ids, ontology, 'freesurfer')
    assert raw.ravel().tolist() == ontology['raw_ids']
    mapping = {'raw_to_neuro': {'17': 47, '53': 48, '1028': 1, '2028': 2}}
    neuro = m.map_labels(ids, ontology, 'neurosegmenter', mapping)
    assert neuro.ravel().tolist() == [0, 0, 47, 48, 1, 2]
    with pytest.raises(ValueError):
        m.map_labels(np.array([96]), ontology, 'freesurfer')
    with pytest.raises(ValueError):
        m.map_labels(np.array([1.5]), ontology, 'freesurfer')


def test_native_restore_uses_rigid_grid_then_native_header():
    native_affine = np.diag([2., 2., 2., 1.])
    rigid_affine = native_affine.copy(); rigid_affine[0, 3] = 20
    data = np.zeros((5, 5, 5), dtype=np.uint8); data[2, 1, 3] = 4
    task = nib.Nifti1Image(data, rigid_affine)
    native = nib.Nifti1Image(np.zeros(data.shape), native_affine)
    restored = m.restore_native(task, native, rigid_affine)
    np.testing.assert_array_equal(restored.get_fdata(), data)
    np.testing.assert_array_equal(restored.affine, native_affine)
    with pytest.raises(ValueError):
        m.restore_native(task, native, np.zeros((4, 4)))


def test_output_never_overwrites_existing_input_or_result(tmp_path):
    original = tmp_path / 'pet.nii.gz'; original.write_bytes(b'original PET')
    alias = tmp_path / 'alias.nii.gz'; alias.symlink_to(original)
    for target in [original, alias]:
        with pytest.raises(FileExistsError):
            m.ensure_new_output(target)
    assert original.read_bytes() == b'original PET'
    m.ensure_new_output(tmp_path / 'new_seg.nii.gz')


def test_native_restore_resamples_a_different_task_grid():
    native_affine = np.diag([2., 2., 2., 1.])
    rigid_affine = native_affine.copy(); rigid_affine[:3, 3] = [20, -10, 6]
    task_affine = rigid_affine.copy(); task_affine[:3, 3] += [-2, -4, 0]
    task_data = np.zeros((8, 9, 7), dtype=np.uint8); task_data[3, 3, 3] = 53
    task = nib.Nifti1Image(task_data, task_affine)
    native = nib.Nifti1Image(np.zeros((5, 5, 5)), native_affine)
    result = m.restore_native(task, native, rigid_affine)
    expected = np.zeros(native.shape, dtype=np.int16); expected[2, 1, 3] = 53
    np.testing.assert_array_equal(result.get_fdata(), expected)
