"""DCCC Seg ONNX inference using official nnU-Net preprocessing and tiling.

The ONNX graph contains only the segmentation network. Geometry and categorical
label restoration are explicit here. No training checkpoint is needed at runtime.
"""
from __future__ import annotations
import argparse
import json
import logging
import os
from pathlib import Path
import time

import nibabel as nib
from nibabel.processing import resample_from_to
import numpy as np

LOG = logging.getLogger('dccc_seg')


def read(path):
    return json.loads(Path(path).read_text())


def ensure_new_output(path):
    path = Path(path)
    if path.exists() or path.is_symlink():
        raise FileExistsError(f'Refusing to overwrite an existing image: {path}')
    if not path.parent.is_dir():
        raise NotADirectoryError(f'Output parent must already exist: {path.parent}')


def valid_affine(affine):
    affine = np.asarray(affine, dtype=float)
    if (affine.shape != (4, 4) or not np.isfinite(affine).all()
            or not np.allclose(affine[3], [0, 0, 0, 1])
            or abs(np.linalg.det(affine[:3, :3])) < 1e-10):
        raise ValueError('Invalid spatial affine')
    return affine


def map_labels(ids, ontology, mode='freesurfer', mapping=None):
    ids = np.asarray(ids)
    raw_ids = np.asarray(ontology['raw_ids'], dtype=np.int16)
    if (not np.isfinite(ids).all() or np.any(ids != np.rint(ids))
            or np.any(ids < 0) or np.any(ids >= len(raw_ids))):
        raise ValueError('Nonintegral or unknown training class')
    ids = ids.astype(np.intp)
    if mode == 'training':
        return ids.astype(np.int16)
    raw = raw_ids[ids]
    if mode == 'freesurfer':
        return raw
    if mode != 'neurosegmenter' or mapping is None:
        raise ValueError('Unknown label mode or missing NeuroSegmenter mapping')
    # Whole brainstem 16 cannot be honestly called brainstem-without-pons 45.
    result = np.zeros(raw.shape, dtype=np.int16)
    for source, target in mapping['raw_to_neuro'].items():
        result[raw == int(source)] = int(target)
    return result


def restore_native(task, native, rigid_affine):
    """Sample task labels in rigid coordinates, then recover native coordinates."""
    rigid_affine = valid_affine(rigid_affine)
    restored = resample_from_to(task, (native.shape[:3], rigid_affine),
                               order=0, mode='constant', cval=0)
    values = np.asarray(restored.dataobj)
    if not np.isfinite(values).all() or np.any(values != np.rint(values)):
        raise ValueError('Native restoration produced invalid categories')
    header = native.header.copy(); header.set_data_dtype(np.int16)
    result = nib.Nifti1Image(values.astype(np.int16), native.affine, header)
    result.set_qform(native.get_qform(), int(native.header['qform_code']))
    result.set_sform(native.get_sform(), int(native.header['sform_code']))
    return result


def make_predictor(bundle, provider='CPUExecutionProvider', threads=4):
    """Reuse official Gaussian sliding windows and accumulation, without flips."""
    bundle = Path(bundle)
    manifest = read(bundle/'manifest.json')
    if manifest.get('mirroring_axes') != [] or manifest.get('default_tta') is not False:
        raise ValueError('This no-flip model requires empty mirroring axes and default_tta=false')
    os.environ['nnUNet_compile'] = 'false'
    import onnxruntime as ort
    import torch
    from nnunetv2.inference.predict_from_raw_data import nnUNetPredictor
    from nnunetv2.utilities.plans_handling.plans_handler import PlansManager
    torch.set_num_threads(threads)
    if provider not in ort.get_available_providers():
        raise RuntimeError(f'Unavailable ONNX provider {provider}; available: {ort.get_available_providers()}')
    options = ort.SessionOptions(); options.intra_op_num_threads = threads
    options.inter_op_num_threads = 1
    session = ort.InferenceSession(str(bundle/'dccc_seg.onnx'), sess_options=options,
                                   providers=[provider])
    class Network(torch.nn.Module):
        def forward(self, x):
            output = session.run(['logits'], {'image': x.detach().cpu().numpy().astype(np.float32)})[0]
            return torch.from_numpy(output)
    plans = PlansManager(read(bundle/'plans.json'))
    config = plans.get_configuration('3d_fullres')
    predictor = nnUNetPredictor(device=torch.device('cpu'), perform_everything_on_device=False,
                               use_mirroring=False, tile_step_size=0.5, use_gaussian=True,
                               verbose=False, allow_tqdm=True)
    predictor.manual_initialization(Network(), plans, config, [{}], read(bundle/'dataset.json'),
                                    manifest['trainer_name'], tuple(manifest['mirroring_axes']))
    return predictor


def segment(input_path, output_path, bundle, work_dir, rigid_header=None,
            rigid_checkpoint=None, repo=None, mode='freesurfer',
            provider='CPUExecutionProvider', threads=4):
    """Segment one 3-D native PET. Exactly one rigid source is mandatory.

    rigid_header must be an index-preserving, two-iteration DCCC rigid result
    for this very input. Integration code owns that identity contract.
    """
    import torch
    from nnunetv2.preprocessing.preprocessors.default_preprocessor import DefaultPreprocessor
    from nnunetv2.inference.export_prediction import convert_predicted_logits_to_segmentation_with_correct_shape
    ensure_new_output(output_path)
    if mode not in ['freesurfer', 'training', 'neurosegmenter']:
        raise ValueError('Unknown label mode')
    if mode == 'neurosegmenter':
        LOG.warning('Partial NeuroSegmenter mapping: brainstem/pons IDs 45/46 unavailable; pons SUVR unsupported')
    if threads < 1:
        raise ValueError("threads must be positive")
    torch.set_num_threads(threads)
    bundle = Path(bundle); work_dir = Path(work_dir)
    work_dir.mkdir(parents=True, exist_ok=False, mode=0o700)
    if work_dir.stat().st_mode & 0o077:
        raise PermissionError('Work directory must be private')
    if (rigid_header is None) == (rigid_checkpoint is None):
        raise ValueError('Supply exactly one of rigid_header or rigid_checkpoint')
    native = nib.load(input_path)
    if len(native.shape) != 3:
        raise ValueError('A single 3-D PET volume is required')
    values = np.asarray(native.dataobj, dtype=np.float32)
    if not np.isfinite(values).all() or not np.any(values > 0):
        raise ValueError('PET must contain finite intensities and positive signal')
    if rigid_checkpoint is not None:
        import sys
        if repo is None:
            raise ValueError('repo is required with rigid_checkpoint')
        sys.path.insert(0, str(Path(repo).resolve()))
        from projects.dccc_seg.pipeline.segmentation_checkpoint import load_rigid_model, iterative_rigid_normalize
        LOG.info('Running two DCCC iterative-rigid iterations on CPU')
        model = load_rigid_model(Path(rigid_checkpoint), torch.device('cpu'))
        aligned, _ = iterative_rigid_normalize(Path(input_path), work_dir/'rigid_header.nii.gz',
                                               model, torch.device('cpu'), 2, 0.)
        del model
    else:
        aligned = nib.load(rigid_header)
        if aligned.shape != native.shape:
            raise ValueError('Rigid result must preserve native voxel indexing')
        # Verify actual index preservation; a same-shaped resampled volume is invalid.
        aligned_values = np.asarray(aligned.dataobj, dtype=np.float32)
        if not np.allclose(values, aligned_values, rtol=1e-6, atol=1e-6):
            raise ValueError('Rigid header image values do not match native voxel indexing')
    rigid_affine = valid_affine(aligned.affine)
    grid = read(bundle/'grid.json')
    task = resample_from_to(nib.Nifti1Image(values, rigid_affine),
                            (tuple(grid['shape']), valid_affine(grid['affine'])), order=1,
                            mode='constant', cval=0)
    task_path = work_dir/'pet_task_0000.nii.gz'; nib.save(task, task_path)
    LOG.info('Materialized training grid; running official preprocessing')
    predictor = make_predictor(bundle, provider, threads)
    data, _, properties = DefaultPreprocessor(verbose=False).run_case(
        [str(task_path)], None, predictor.plans_manager,
        predictor.configuration_manager, predictor.dataset_json)
    np.save(work_dir/'preprocessed_input.npy', data)
    LOG.info('Predicting %s with ONNX (%s), one unflipped prediction', data.shape, provider)
    logits = predictor.predict_logits_from_preprocessed_data(torch.from_numpy(data))
    if not torch.isfinite(logits).all():
        raise RuntimeError('Nonfinite network logits')
    labels = convert_predicted_logits_to_segmentation_with_correct_shape(
        logits, predictor.plans_manager, predictor.configuration_manager,
        predictor.label_manager, properties, num_threads_torch=threads)
    task_output = work_dir/'training_ids_task.nii.gz'
    predictor.plans_manager.image_reader_writer_class().write_seg(labels, str(task_output), properties)
    task_labels = nib.load(task_output)
    if task_labels.shape != task.shape or not np.allclose(task_labels.affine, task.affine, atol=1e-4):
        raise RuntimeError('Official output did not return to the verified training grid')
    restored = restore_native(task_labels, native, rigid_affine)
    ontology = read(bundle/'labels.json'); mapping = read(bundle/'neurosegmenter_mapping.json')
    restored.set_data_dtype(np.int16)
    mapped = map_labels(np.asarray(restored.dataobj), ontology, mode, mapping)
    output = nib.Nifti1Image(mapped, native.affine, restored.header)
    ensure_new_output(output_path)
    nib.save(output, output_path)
    receipt = {'schema': 'dccc_seg_native_onnx_v1', 'input': str(Path(input_path).resolve()),
               'output': str(Path(output_path).resolve()), 'mode': mode,
               'native_shape': list(native.shape), 'native_affine': native.affine.tolist(),
               'rigid_affine': rigid_affine.tolist(), 'task_grid': grid,
               'provider': provider, 'tta': False, 'tile_step_size': 0.5,
               'neurosegmenter_limitations': mapping['limitations'] if mode == 'neurosegmenter' else [],
               'completed_unix': time.time()}
    (work_dir/'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
    LOG.info('Saved native segmentation: %s', output_path)
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--bundle', required=True, type=Path)
    parser.add_argument('--work-dir', required=True, type=Path, help='New private directory on HDD')
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--rigid-header', type=Path)
    group.add_argument('--rigid-checkpoint', type=Path)
    parser.add_argument('--repo', type=Path)
    parser.add_argument('--mode', choices=['freesurfer', 'training', 'neurosegmenter'], default='freesurfer')
    parser.add_argument('--provider', default='CPUExecutionProvider')
    parser.add_argument('--threads', type=int, default=4)
    args = parser.parse_args()
    if args.threads < 1: parser.error('--threads must be positive')
    logging.basicConfig(level=logging.INFO, format='%(asctime)s %(levelname)s %(message)s')
    os.umask(0o077)
    segment(args.input, args.output, args.bundle, args.work_dir, args.rigid_header,
            args.rigid_checkpoint, args.repo, args.mode, args.provider, args.threads)


if __name__ == '__main__':
    main()
