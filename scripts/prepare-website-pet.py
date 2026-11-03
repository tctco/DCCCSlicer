#!/usr/bin/env python3
"""Prepare local PET surface illustrations; input volumes stay in ignored cache.

Requires numpy, scipy, nibabel, trimesh, fast-simplification, SimpleITK, matplotlib
and the repository's dcccpy on PYTHONPATH. See website/README.md.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import json
from pathlib import Path
import zipfile

import nibabel as nib
import numpy as np
from scipy.io import loadmat
from scipy.ndimage import map_coordinates
import trimesh


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def sample(image, points, order=1):
    vox = nib.affines.apply_affine(np.linalg.inv(image.affine), points)
    return map_coordinates(image.get_fdata(dtype=np.float32), vox.T, order=order,
                           mode='constant', cval=0, prefilter=False)


def register_template(root, cache, template):
    import SimpleITK as sitk
    mri = loadmat(root / 'subjectimage_T1.mat', simplify_cells=True)
    affine = np.eye(4)
    affine[:3, 3] = [-90, -126, -72]
    brain = sum(nib.load(root / f'hm_colin27/mask_{t}.nii').get_fdata()
                for t in ['gray', 'white', 'csf']) > 0
    cube = mri['Cube'].astype('float32')
    cube[~brain] = 0
    fixed_path = cache / 'colin27_T1_brain.nii.gz'
    nib.save(nib.Nifti1Image(cube, affine), fixed_path)
    sitk.ProcessObject.SetGlobalDefaultNumberOfThreads(4)
    fixed = sitk.ReadImage(str(fixed_path), sitk.sitkFloat32)
    moving = sitk.ReadImage(str(template), sitk.sitkFloat32)
    transform = sitk.AffineTransform(3)
    reg = sitk.ImageRegistrationMethod()
    reg.SetMetricAsMattesMutualInformation(48)
    reg.SetMetricSamplingStrategy(reg.RANDOM)
    reg.SetMetricSamplingPercentage(.2, seed=481)
    reg.SetInterpolator(sitk.sitkLinear)
    reg.SetOptimizerAsGradientDescent(learningRate=.3, numberOfIterations=160,
                                    convergenceMinimumValue=1e-6, convergenceWindowSize=12)
    reg.SetOptimizerScalesFromPhysicalShift()
    reg.SetShrinkFactorsPerLevel([4, 2, 1])
    reg.SetSmoothingSigmasPerLevel([2, 1, 0])
    reg.SmoothingSigmasAreSpecifiedInPhysicalUnitsOn()
    reg.SetInitialTransform(transform, inPlace=True)
    reg.Execute(fixed, moving)
    sitk.WriteTransform(transform, str(cache / 'colin_to_mni_lps.tfm'))
    matrix = np.eye(4)
    matrix[:3, :3] = np.array(transform.GetMatrix()).reshape(3, 3)
    matrix[:3, 3] = transform.GetTranslation()
    flip = np.diag([-1., -1, 1, 1])
    matrix = flip @ matrix @ flip
    np.save(cache / 'colin_to_mni_ras.npy', matrix)
    sitk.WriteImage(sitk.Resample(moving, fixed, transform, sitk.sitkLinear, 0),
                    str(cache / 'mni_T1_in_colin.nii.gz'))
    return matrix


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path, required=True)
    parser.add_argument('--colin', type=Path, required=True)
    parser.add_argument('--core', type=Path, required=True)
    parser.add_argument('--cache', type=Path, default=Path('.cache/website-pet'))
    parser.add_argument('--output', type=Path, default=Path('website/assets/pet'))
    args = parser.parse_args()
    import dcccpy
    core = args.core.resolve()
    cache = args.cache.resolve()
    output = args.output.resolve()
    cache.mkdir(parents=True, exist_ok=True)
    output.mkdir(parents=True, exist_ok=True)
    native_version = dcccpy.run(['--version'], executable=core).stdout.strip()
    print(f'Explicit runtime: {native_version}; wrapper expected: {dcccpy.DCCCCORE_VERSION}', flush=True)
    # All native images and original metadata are local-only and ignored by Git.
    with zipfile.ZipFile(args.archive) as archive:
        for name in archive.namelist():
            if name.startswith('PET/') or name in ('README.txt', 'case_manifest.csv'):
                dest = cache / name
                if dest.resolve().is_relative_to(cache) and not dest.exists():
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    dest.write_bytes(archive.read(name))
    rows = list(csv.DictReader((cache / 'case_manifest.csv').open()))
    rows.sort(key=lambda row: float(row['actual_cl']))
    normalized = cache / 'normalized'
    normalized.mkdir(exist_ok=True)
    for row in rows:
        src = cache / 'PET' / row['filename']
        dest = normalized / row['filename']
        if not dest.exists():
            print('Normalizing', src.name, flush=True)
            result = dcccpy.normalize(src, dest, iterative=True, executable=core)
            (normalized / (src.name + '.log')).write_text(result.stdout + '\n' + result.stderr)
        else:
            nib.load(dest)  # Refuse damaged cached outputs.
    template = core.parent / 'assets/nii/padded_MNI152_T1_2mm.nii'
    transform_file = cache / 'colin_to_mni_ras.npy'
    matrix = (np.load(transform_file) if transform_file.exists()
              else register_template(args.colin, cache, template))
    mri = loadmat(args.colin / 'subjectimage_T1.mat', simplify_cells=True)
    cortex_path = args.colin / 'tess_cortex_pial_high.mat'
    cortex = loadmat(cortex_path, simplify_cells=True)
    vertices = (cortex['Vertices'] * 1000 - mri['SCS']['T']) @ mri['SCS']['R'] - [91, 127, 73]
    faces = cortex['Faces'].astype(np.int32) - 1
    mesh = trimesh.Trimesh(vertices, faces, process=False)
    # Preserve the folded Colin27 anatomy while reducing download and GPU costs.
    mesh = mesh.simplify_quadric_decimation(face_count=120000)
    mesh.fix_normals(multibody=True)
    vertices = np.asarray(mesh.vertices)
    normals = np.asarray(mesh.vertex_normals)
    assert np.isfinite(vertices).all() and mesh.is_winding_consistent
    print('Mesh:', len(vertices), 'vertices;', len(mesh.faces), 'faces', flush=True)
    gray = nib.load(args.colin / 'hm_colin27/mask_gray.nii')
    # Cortical MIP: maximum over inward samples with gray-matter support.
    # This is a finite normal-ray projection, not a full-head camera-axis MIP.
    depths = np.arange(.5, 6.01, .5)
    ribbon = vertices[None] - depths[:, None, None] * normals[None]
    gray_support = sample(gray, ribbon.reshape(-1, 3)).reshape(len(depths), -1)
    valid = gray_support > .25
    fallback = ~valid.any(axis=0)
    valid[:, fallback] = True
    points = nib.affines.apply_affine(matrix, ribbon.reshape(-1, 3))
    ref = nib.load(core.parent / 'assets/nii/voi_WhlCbl_2mm.nii')
    ref_vox = np.array(np.where(ref.get_fdata() > .5)).T
    ref_world = nib.affines.apply_affine(ref.affine, ref_vox)
    values, cases, local_receipts = [], [], []
    for row in rows:
        image = nib.load(normalized / row['filename'])
        volume = image.get_fdata(dtype=np.float32)
        assert volume.ndim == 3 and np.isfinite(volume).all()
        denominator = float(sample(image, ref_world).mean())
        if denominator <= 0:
            raise ValueError('Invalid whole-cerebellum reference mean')
        samples = sample(image, points).reshape(len(depths), -1)
        suvr = np.max(np.where(valid, samples, -np.inf), axis=0) / denominator
        if not np.isfinite(suvr).all() or (suvr > 0).mean() < .95:
            raise ValueError('Insufficient surface sampling coverage')
        quantized = np.round(np.clip(suvr, 0, 6) * 10000).astype('<u2')
        values.append(quantized)
        cases.append({'actualCL': float(row['actual_cl']), 'targetCL': float(row['target_cl']),
                      'tracer': row['tracer'], 'conversion': row['conversion']})
        local_receipts.append({'filename': row['filename'], 'sourceSHA256': digest(cache / 'PET' / row['filename']),
                               'referenceMean': denominator, 'surfaceSUVrPercentiles': np.percentile(suvr, [0, 5, 50, 95, 100]).tolist(),
                               'positiveSampleFraction': float((suvr > 0).mean())})
        print('CL', row['actual_cl'], 'SUVr', np.round(np.percentile(suvr, [5, 50, 95]), 3), flush=True)
    # Geometric cavity shading supports the ink-on-paper treatment.
    neighbors = mesh.vertex_neighbors
    laplacian = np.array([vertices[n].mean(axis=0) - v for v, n in zip(vertices, neighbors)])
    curvature = np.einsum('ij,ij->i', laplacian, normals)
    cavity = np.clip(.93 - curvature * .22, .55, 1)
    (output / 'positions.bin').write_bytes(vertices.astype('<f4').tobytes())
    (output / 'normals.bin').write_bytes(normals.astype('<f4').tobytes())
    (output / 'indices.bin').write_bytes(np.asarray(mesh.faces, dtype='<u4').tobytes())
    (output / 'cavity.bin').write_bytes(np.round(cavity * 255).astype('u1').tobytes())
    (output / 'uptake.bin').write_bytes(np.stack(values).tobytes())
    manifest = {
        'version': 1, 'vertexCount': len(vertices), 'triangleCount': len(mesh.faces),
        'cases': cases, 'units': 'whole-cerebellum-referenced SUVr',
        'uptakeEncoding': 'case-major uint16 little-endian; divide by 10000',
        'surface': 'Brainstorm Colin27 2016 pial, decimated to 120000 faces',
        'surfaceSource': 'https://neuroimage.usc.edu/brainstorm/Tutorials/DefaultAnatomy',
        'surfaceSHA256': digest(cortex_path), 'coordinateSystem': 'Colin27 RAS millimeters',
        'normalization': {'runtime': native_version, 'wrapperExpectedVersion': dcccpy.DCCCCORE_VERSION,
                          'method': 'rigid_voxelmorph, iterative rigid enabled',
                          'outputSpace': 'DCCC MNI template grid'},
        'templateAlignment': 'Affine Mattes MI: Colin27 T1 to padded MNI152 T1; ITK LPS transformed explicitly to RAS',
        'colinToMNIRAS': matrix.tolist(),
        'sampling': 'Cortical MIP over 12 inward normal samples at 0.5–6.0 mm, gray-matter support > 0.25; trilinear volume sampling',
        'graySupportFallbackFraction': float(fallback.mean()),
        'visualization': 'Continuous linear interpolation of projected SUVr between nominal 0–100 CL selection targets; whole-number presentation scale. Actual source CL values are retained separately. No synthetic clinical measurements.',
        'displayWindowSUVr': [1.1, 1.75],
        'overlay': 'Transparent below 1.1 SUVr; red opacity increases smoothly to 0.86 at 1.75 SUVr over a neutral anatomical surface',
        'limitations': ['Distinct participants, not longitudinal progression.',
                       'PiB and AV45 are different tracers; common SUVr colors are not a validated cross-tracer calibration.',
                       'Source actual CL values are supplied records (dcccpy 0.2.0 / runtime 4.4.0); not recalculated here.',
                       'Projection, red-overlay display thresholds and interpolation are for visual communication; not a diagnostic reference.'],
        'files': {}
    }
    for name in ['positions.bin', 'normals.bin', 'indices.bin', 'cavity.bin', 'uptake.bin']:
        manifest['files'][name] = {'bytes': (output / name).stat().st_size, 'sha256': digest(output / name)}
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (cache / 'processing-receipt.json').write_text(json.dumps(local_receipts, indent=2) + '\n')
    print('Exported surface-only assets:', output, flush=True)


if __name__ == '__main__':
    main()
