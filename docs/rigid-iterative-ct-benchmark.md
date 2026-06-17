# Rigid Iterative CT Benchmark

Date: 2026-06-16

Input:

- `/workspace/localizer/Testing/CT.nii.gz`

Build:

```bash
docker compose -f docker-compose.core.yml run --rm dccc-core /workspace/scripts/docker-build-core.sh
```

Benchmark command shape:

```bash
docker compose -f docker-compose.core.yml run --rm dccc-core \
  python3 -c '... subprocess.run(["./install/bin/DCCCcore", "rigid", "--input", "/workspace/localizer/Testing/CT.nii.gz", "--output", "/tmp/dccc_ct_rigid_*.nii", "--iterative"]) ...'
```

`/usr/bin/time` is not installed in the development container, so the benchmark used Python `time.perf_counter()` for wall time and `resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss` for peak child-process RSS.

## Baseline

Baseline command:

```bash
./install/bin/DCCCcore rigid \
  --input /workspace/localizer/Testing/CT.nii.gz \
  --output /tmp/dccc_ct_rigid_baseline.nii \
  --iterative
```

Result:

- Return code: 0
- Wall time: 32.233 seconds
- Max RSS: 6,810,932 KB

The slow path was not caused by repeatedly reading the input file from disk. The input is loaded once. The main issues were:

- `rigidOnly` requests still computed VoxelMorph spatial normalization, then discarded it.
- Rigid preprocessing computed intensity percentiles by sorting all high-resolution voxels as `double`.
- The iterative loop wrote `rigid_iter.nii` every pass, but that file was never read.

## Changes Tested

Implemented changes:

- Added rigid-only normalization entry points so the `rigid` command stops before VoxelMorph.
- Removed the unused per-iteration `rigid_iter.nii` temporary write.
- Replaced full percentile sorting with `std::nth_element` over a `float` voxel buffer.

An attempted low-resolution iterative working-image cache was rejected. It improved speed, but changed the final affine too much on `CT.nii.gz`; the old/new sform difference reached 19.5 mm in translation. The final implementation therefore preserves the original high-resolution iterative affine-estimation semantics.

## Results

Rejected low-resolution-cache experiment:

- Return code: 0
- Wall time: 16.848 seconds
- Max old/new sform absolute difference: 19.525314 mm

Final result:

- Return code: 0
- Wall time: 16.991 seconds
- Max old/new sform absolute difference: 0.0
- Max old/new qoffset absolute difference: 0.0
- Max old/new quaternion absolute difference: 0.0
- Max RSS: 6,241,604 KB

An earlier final timing run before the affine correction measured 16.848 seconds and max RSS 6,603,216 KB, but that variant is not valid because the affine changed.

After rigid-only branching and removal of the low-resolution-cache experiment, the accepted output preserves the old affine exactly for this CT benchmark while still avoiding the discarded VoxelMorph work.

Earlier intermediate result after rigid-only branching and low-resolution iterative working image, before the percentile change:

- Return code: 0
- Wall time: 23.528 seconds
- Max RSS: 6,543,312 KB

This intermediate low-resolution result is included only for auditability and is not the accepted implementation.

Compared with baseline:

- Wall time improved by about 47.3%.
- Peak RSS remained high because the original high-resolution CT image and ITK processing buffers are still held during the first rigid pass.

## Verification

Commands run:

```bash
docker compose -f docker-compose.core.yml run --rm dccc-core /workspace/scripts/docker-build-core.sh
docker compose -f docker-compose.core.yml run --rm dccc-core pytest tests/test_normalize_cli.py -q
```

Test result:

- `tests/test_normalize_cli.py`: 4 passed in 41.85 seconds

## Notes

Further speed or memory reductions would require changing the iterative model input or the first rigid preprocessing pass so that very high-resolution images are downsampled before percentile clipping and smoothing. The low-resolution-cache experiment shows that this is a behavioral change and must be validated against affine accuracy before acceptance.
