# Deep Cascaded Cerebral Calculator Core

A deep learning-based multimodal brain PET image standardization and semi-quantitative analysis software for neuroimaging biomarker calculation.

## Overview

The Deep Cascaded Cerebral Calculator Core is a comprehensive C++ toolkit designed for automated quantitative analysis of brain PET images. It provides standardized spatial normalization and calculates various semi-quantitative metrics including Centiloid, CenTauR, CenTauRz, and custom SUVr values. The software employs deep learning models for accurate spatial registration and supports multiple PET tracers for both amyloid and tau imaging.

### Key Features

- **Multi-biomarker Support**: Calculates Centiloid (amyloid), AbetaIndex/AbetaLoad (amyloid), CenTauR/CenTauRz (tau), fill-states, and custom SUVr metrics
- **Deep Learning Pipeline**: Utilizes ONNX-based neural networks for spatial normalization
- **Modular Architecture**: Extensible design with clean interfaces for adding new biomarkers
- **Multi-tracer Compatibility**: Supports various PET tracers with tracer-specific calibrations
- **ADAD Scoring**: Built-in decoupling pipeline for abeta/tau modalities via `adad`
- **Configuration-driven**: Flexible TOML-based configuration system

## Usage

The software provides a command-line interface with multiple subcommands for different analysis tasks.

### Native PET segmentation (`seg`)

```sh
./DCCCcore seg --input pet.nii.gz --output labels.nii.gz
```

`seg` uses the bundled DCCC Seg 96-class ONNX model. It performs exactly two
rigid predictions, samples the verified 1.5 mm training grid, applies nnU-Net
cropping/Z-score normalization and one unflipped Gaussian prediction,
then restores signed 16-bit categorical labels to the original PET geometry.
The production command does not require Python, PyTorch or nnU-Net.
The bound model was trained without flips. No TTA or flip option is exposed;
legacy switches are rejected rather than ignored. FP16 weights use FP32
network arithmetic and float32 input/output tensors.

| Option | Meaning |
| --- | --- |
| `--mode freesurfer` | Default: reversible raw DKT/aseg IDs |
| `--mode training` | Contiguous training IDs 0–95 |
| `--mode neurosegmenter` | Partial regional aggregation; no IDs 45/46 or pons SUVR |
| `--threads 4` | CPU thread count; must be positive |
| `--bundle DIRECTORY` | Override `assets/models/seg` next to the executable |
| `--rigid-header FILE` | Two-step rigid header with exactly the input's voxel values; skips internal rigid |
| `--debug` | Save rigid header, task PET, normalized crop, task training IDs and JSON receipt |

Input must be one finite 3-D PET with positive signal. The output parent must
exist and the output must be new. Batch/BIDS inputs are unsupported. Training
IDs and FreeSurfer IDs are different vocabularies; consult the bundled
`labels.json` and `neurosegmenter_mapping.json` before interpreting a label.
The original bundle README documents provenance and reference validation.

In DCCCSlicer, select the PET input volume and use **PET segmentation → Segment
PET**. The command runs asynchronously and loads a named categorical labelmap
over the original volume. In Python use `dcccpy.seg("pet.nii.gz", "labels.nii.gz")`.

For integration parity checks inside a Docker environment containing nnU-Net:

```sh
python scripts/validate-seg-parity.py \
  --core localizer/src/install-seg/bin/DCCCcore \
  --bundle localizer/src/assets/models/seg \
  --input localizer/src/tests/test_batch/input.nii \
  --work-dir /new/private/seg-parity-case
```

This retains QC artifacts and reports preprocessing errors and native-label
agreement against the shipped Python reference, conditioned on the native C++
rigid header. It verifies two rigid predictions and voxel-index preservation;
independent rigid transform parity requires the original checkpoint/pipeline,
which is not in the bundle. It does not establish clinical accuracy or replace
visual review in Slicer.

### Basic Commands

#### Centiloid Analysis
Calculate standardized amyloid burden scores using the refactored CLI:

```bash
# Basic Centiloid calculation
./DCCCcore centiloid --input amyloid_pet.nii --output result.nii

# With configuration file
./DCCCcore centiloid --input amyloid_pet.nii --output result.nii --config custom_config.toml

# Include SUVr values in output
./DCCCcore centiloid --input amyloid_pet.nii --output result.nii --suvr

# Skip spatial normalization (pre-normalized images)
./DCCCcore centiloid --input normalized_pet.nii --output result.nii --skip-normalization
```

Batch processing of multiple amyloid PET images:

```bash
# Process all .nii / .nii.gz files under input_dir and write outputs to output_dir
./DCCCcore centiloid --input input_dir --output output_dir --batch
```

#### CenTauR Analysis
Calculate standardized tau burden scores:

```bash
# CenTauR percentile scale
./DCCCcore centaur --input tau_pet.nii --output result.nii

# CenTauRz z-score scale
./DCCCcore centaurz --input tau_pet.nii --output result.nii
```

#### AbetaIndex Analysis
Calculate the AV45-only AbetaIndex coefficient using the `mean`, `PC1`, and fixed-`PC2` templates:

```bash
./DCCCcore abetaindex --input amyloid_pet.nii --output result.nii

# Batch mode
./DCCCcore abetaindex --input input_dir --output output_dir --batch
```

#### Fill-states Analysis
Calculate fill-states metric based on voxel-wise z-scores within tracer-specific meta-ROIs:

```bash
# Amyloid tracer (FBP) fill-states
./DCCCcore fillstates --input amyloid_pet.nii --output result.nii --tracer fbp

# FDG neurodegeneration fill-states
./DCCCcore fillstates --input fdg_pet.nii --output result.nii --tracer fdg

# FTP tau fill-states
./DCCCcore fillstates --input ftp_pet.nii --output result.nii --tracer ftp
```

The command produces:

- A **normalized output** image at the path specified by `--output`, same as other SUVr-derived metrics.
- A **fill_states_map** image saved alongside the output, using the same base name with suffix `_fill_states_map`, containing a 0/1 mask of voxels exceeding the z-score threshold within the meta-ROI.

#### Custom SUVr Calculation
Calculate SUVr with user-defined regions:

```bash
./DCCCcore suvr --input pet.nii --output result.nii \
  --voi-mask target_region.nii --ref-mask reference_region.nii
```

Batch SUVr calculation with user-defined regions:

```bash
./DCCCcore suvr --input input_dir --output output_dir --batch \
  --voi-mask target_region.nii --ref-mask reference_region.nii
```

#### Spatial Normalization Only
Perform spatial standardization without metric calculation:

```bash
# Standard normalization
./DCCCcore normalize --input pet.nii --output normalized.nii

# ADNI-style processing
./DCCCcore adni-pet-core --input amyloid_pet.nii --output normalized.nii --tracer abeta

# FDG uses ADNI's iterative masked global-mean normalization
./DCCCcore adni-pet-core --input fdg_pet.nii --output normalized_fdg.nii --tracer fdg

# Export only motion-corrected dynamic PET (Level 1)
./DCCCcore adni-pet-core --input dynamic_pet.nii.gz --output coreg.nii.gz \
  --tracer fdg --level 1

# Export selected stages only: Level 1 and Level 3 (Level 2 remains temporary)
./DCCCcore adni-pet-core --input dynamic_pet.nii.gz --output standardized.nii.gz \
  --tracer fdg --level 1 3

# Zero facial voxels after estimating transforms from the original data
./DCCCcore adni-pet-core --input dynamic_pet.nii.gz --output defaced.nii.gz \
  --tracer fdg --level 1 3 --deface

# Multi-frame PET motion correction and arithmetic mean
./DCCCcore pet-motion-correct --input dynamic_pet.nii.gz --output averaged_pet.nii.gz

# Optionally retain corrected frames and per-frame rigid motion parameters
./DCCCcore pet-motion-correct --input dynamic_pet.nii.gz --output averaged_pet.nii.gz \
  --save-corrected-dynamic corrected.nii.gz --motion-output motion.tsv

# Iterative rigid registration
./DCCCcore normalize --input pet.nii --output normalized.nii --iterative
./DCCCcore adni-pet-core --input tau_pet.nii --output normalized.nii --tracer tau --iterative
./DCCCcore rigid --input pet.nii --output rigid.nii --iterative
```

### Whole-brain extraction

The inverse-capable VoxelMorph model can warp the bundled MNI brain mask back
to the input voxel grid and use it for skull stripping. The binary mask is
written beside the extracted image as `brain_mask.nii.gz` unless
`--mask-output` is specified.

```bash
./DCCCcore brain-extract --input structural_mri.nii.gz --output brain.nii.gz
./DCCCcore brain-extract --input structural_mri.nii.gz --output brain.nii.gz \
  --mask-output subject_brain_mask.nii.gz
```

### ADNI-style regional volumes

`brain-parcellate` inverse-warps a bundled FreeSurfer fsaverage
Desikan-Killiany + aseg atlas and writes native-space binary masks plus a
`volumes.json` summary. `--output` is an output directory.

```bash
./DCCCcore brain-parcellate --input structural_mri.nii.gz \
  --output regional_volumes
```

The summary includes ventricles, left/right and bilateral hippocampus,
entorhinal cortex, fusiform gyrus and middle temporal gyrus, whole brain, and
an intracranial mask estimate. The reported ICV is an inverse-warped mask
proxy; it is deliberately not called FreeSurfer eTIV because eTIV is derived
from the subject's Talairach transform rather than an `aseg` voxel label.

`pet-motion-correct` accepts general 4D `X × Y × Z × N` PET data. It uses frame 0
as the fixed reference, independently registers every later frame to it with a
six-degree-of-freedom rigid transform, and writes the arithmetic mean as a 3D
float NIfTI. `adni-pet-core --level` accepts any combination of the following
stages (default: `3`):

1. `Coreg`: motion-corrected 4D dynamic PET; requires a 4D input.
2. `Coreg, Avg`: the averaged 3D PET. A 3D input is treated as already averaged.
3. `Coreg, Avg, Std Img and Vox Siz`: the existing tracer-specific standardized output.

Only explicitly selected levels are retained. The pipeline calculates required
intermediate data but does not export it; for example, `--level 1 3` writes
Levels 1 and 3 but not Level 2. Processing stops after the highest requested
level, so `--level 1` does not allocate an average image or initialize spatial
normalization, while `--level 1 2` stops before spatial normalization. For a
single input, `--output` names the highest selected level; any selected lower
outputs use `_Coreg` or `_Coreg_Avg` before the NIfTI extension. Batch outputs
use `_Coreg.nii`, `_Coreg_Avg.nii`, and `_ADNI_style.nii` respectively.

`adni-pet-core` requires
`--tracer abeta`, `--tracer tau`, `--tracer fdg`, or `--tracer dat`. Aβ and tau
use cerebellar gray normalization. DAT follows the PPMI-style workflow and uses
the occipital lobe as its Level 3 intensity reference. The bundled reference
mask uses the MNI152 2 mm grid. FDG uses the restored ADNI iterative global-mean
procedure: scale the whole image to mean 1, repeatedly exclude voxels below
0.5 and rescale retained voxels to mean 1 until the excluded count is unchanged.

`scan-pet-core --tracer fdg` selects the separate SCAN-inspired reference:
the mean of the highest 50% of PET intensities in a pons/cerebellar-vermis
union (ceil(N/2) voxels). The divisor is measured in spatially normalized PET
and applied to the existing rigid-aligned ADNI-grid output. SCAN currently
accepts only FDG. Registration, motion correction, Level 1/2 exports and defacing
share the ADNI implementation; only the intensity strategy differs. All export
levels and batch/BIDS controls are supported. The SCAN batch Level 3 suffix is
`_SCAN_style.nii`.

```bash
./DCCCcore scan-pet-core --input fdg_pet.nii --output scan_fdg.nii --tracer fdg
```

The split is available from `4.6.3-alpha`. The superseded `4.6.2-alpha` package
put the SCAN-inspired FDG reference on `adni-pet-core` and had no
`scan-pet-core` command. Upgrade to restore legacy ADNI behavior.

Motion TSV translations are in millimetres and Euler rotations are in radians.

The bundled FDG mask combines Clinica's eroded pons with AAL2 vermis labels
on DCCC's padded MNI 2 mm grid. It approximates the SCAN reference, rather
than reproducing Berkeley's hand-drawn mask or full SCAN pipeline. Override
`masks.fdg_pons_vermis_ref` in a custom configuration to use another binary
MNI-space reference. See [mask provenance](assets/nii/FDG/README.md) and the
[FDG compatibility note](../../docs/adni-fdg-preprocessing-validation.md).

#### ADAD Analysis
Run the ADAD decoupling-based metric:

```bash
# Default (abeta) modality
./DCCCcore adad --input pet.nii --output result.nii

# Tau modality with iterative rigid alignment
./DCCCcore adad --input pet.nii --output result.nii --modality tau --iterative
```

### Command Options

| Option | Description |
|--------|-------------|
| `--config <file>` | Configuration file path (default: config.toml) |
| `--debug` | Enable debug mode with intermediate outputs |
| `--batch` | Enable batch processing mode. In batch mode, `--input` and `--output` are treated as directories, all `.nii` / `.nii.gz` files in the input directory are processed, and outputs use the input basename plus each command's preset suffix. Metric commands also generate `results.csv`, and batch-capable commands generate `batch_info.txt` in the output directory. Currently supported for `centiloid`, `centaur`, `centaurz`, `suvr`, `abetaindex`, `abetaload`, `rigid`, `adni-pet-core`, and `scan-pet-core`. When registration is enabled (no `--skip-normalization`), the output directory must be empty to avoid overwriting. |
| `--bids <regex>` | Treat `--input` as a PET-BIDS dataset root, recursively process PET NIfTI files under `pet/` whose BIDS relative path, filename, or basename matches `<regex>`, and write outputs to the `--output` directory using the original BIDS basename plus the command suffix, such as `sub-01_ses-01_pet_rigid_aligned.nii` or `sub-01_ses-01_pet_ADNI_style.nii`. |
| `--iterative` | Use iterative rigid transformation |
| `--manual-fov` | Enable manual field-of-view placement |
| `--skip-normalization` | Skip spatial normalization step |
| `--suvr` | Include SUVr values in metric outputs |
| `--tracer <tracer>` | Required tracer type. `adni-pet-core` accepts `abeta`, `tau`, `fdg`, or `dat`; `scan-pet-core` accepts only `fdg`; `fillstates` accepts `fbp`, `fdg`, or `ftp`. |
| `--level <1\|2\|3> [...]` | `adni-pet-core` / `scan-pet-core` export levels. Accepts one or more values and defaults to Level 3. Only selected levels are saved. |
| `--deface` | `adni-pet-core` and `scan-pet-core`. Sets facial voxels to zero in every selected output after transform estimation. |
| `--modality <type>` | Decoupling modality for `adad` (`abeta` or `tau`). |

## Developers

To add your own brain PET metric, please refer to the [developer guide](../../docs/developer_guide.md).

## Docker Development Environment

The C++ core can be built in a Linux Docker container with Conan and CMake. This is the recommended path for reproducible development across machines.

Build the development image:

```bash
docker compose -f docker-compose.core.yml build
```

Build and install the C++ core:

```bash
docker compose -f docker-compose.core.yml run --rm dccc-core /workspace/scripts/docker-build-core.sh
```

For Linux release builds that must run on older distributions without glibc 2.34, use the legacy compatibility container instead. It is based on the manylinux2014 / CentOS 7 toolchain so generated Linux binaries target glibc 2.17, which covers RHEL/CentOS 7-era images and avoids accidental `GLIBC_2.34` symbol requirements:

```bash
docker compose -f docker-compose.core.yml build dccc-core-legacy
docker compose -f docker-compose.core.yml run --rm dccc-core-legacy /workspace/scripts/docker-build-core.sh
```

The installed executable will be written to `localizer/src/install/bin/DCCCcore`. Conan packages and the CMake build tree are kept in Docker volumes so later builds can reuse downloaded and compiled dependencies. The default development container and the legacy compatibility container use separate Conan and CMake volumes to avoid mixing dependency builds from different glibc baselines. The script defaults to `CONAN_CPPSTD=gnu17` on Linux to reuse more Conan Center binaries; use `CONAN_CPPSTD=17` if you need a strict non-GNU C++17 profile. The legacy container also sets `CONAN_BUILD_ARGS="--build=missing --build=b2/* --build=m4/* --build=autoconf/* --build=automake/* --build=libtool/* --build=pkgconf/*"` so native build tools such as Boost's `b2` and autotools packages used by libcurl are compiled inside the manylinux2014 environment instead of downloading ConanCenter binaries that may require newer glibc symbols such as `GLIBC_2.34`.

The first run can still take a long time because Conan Center may not provide matching Linux binaries for heavy packages such as ITK, ONNX Runtime, Boost, HDF5, GDCM, or TBB. Let the first build finish once on a machine, then keep the Docker volumes for normal incremental development.

Run the CLI from an interactive shell:

```bash
docker compose -f docker-compose.core.yml run --rm dccc-core
./install/bin/DCCCcore --help
```

Run Python CLI tests after building:

```bash
docker compose -f docker-compose.core.yml run --rm dccc-core pytest tests
```

To force a clean dependency/build cache, remove the Docker volumes:

```bash
docker compose -f docker-compose.core.yml down -v
```
