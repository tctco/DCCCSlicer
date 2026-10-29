"""Contract tests for label vocabularies and reversible native geometry."""
import importlib.util
from pathlib import Path
import nibabel as nib
import numpy as np
import pytest
import inspect
import json
import subprocess
import sys

spec = importlib.util.spec_from_file_location('dccc_seg', Path(__file__).with_name('dccc_seg.py'))
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


@pytest.mark.parametrize('tta', [True, False])
def test_reference_interfaces_do_not_accept_tta(tta):
    for function, args in [(m.make_predictor, [Path('.')]),
                           (m.segment, ['input', 'output', Path('.'), 'work'])]:
        with pytest.raises(TypeError, match='tta'):
            inspect.signature(function).bind(*args, tta=tta)


@pytest.mark.parametrize('flag', ['--tta', '--no-tta', '--flip'])
def test_reference_cli_rejects_flip_flags(flag, tmp_path):
    result = subprocess.run([sys.executable, str(Path(m.__file__)), '--input', 'missing.nii',
        '--output', str(tmp_path/'out.nii'), '--bundle', str(tmp_path), '--work-dir',
        str(tmp_path/'work'), '--rigid-header', 'missing.nii', flag], capture_output=True, text=True)
    assert result.returncode == 2
    assert 'unrecognized arguments' in result.stderr
    assert flag in result.stderr


@pytest.mark.parametrize('manifest', [
    {'mirroring_axes': [0], 'default_tta': False},
    {'mirroring_axes': [], 'default_tta': True},
    {'mirroring_axes': [], 'default_tta': 'false'},
    {'mirroring_axes': []},
])
def test_reference_rejects_incompatible_manifest_before_session(tmp_path, manifest):
    (tmp_path/'manifest.json').write_text(json.dumps(manifest))
    with pytest.raises(ValueError, match='no-flip'):
        m.make_predictor(tmp_path)


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
