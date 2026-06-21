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
./DCCCcore adni-pet-core --input pet.nii --output normalized.nii

# Iterative rigid registration
./DCCCcore normalize --input pet.nii --output normalized.nii --iterative
./DCCCcore adni-pet-core --input pet.nii --output normalized.nii --iterative
./DCCCcore rigid --input pet.nii --output rigid.nii --iterative
```

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
| `--batch` | Enable batch processing mode. In batch mode, `--input` and `--output` are treated as directories, all `.nii` / `.nii.gz` files in the input directory are processed, and outputs use the input basename plus each command's preset suffix. Metric commands also generate `results.csv`, and batch-capable commands generate `batch_info.txt` in the output directory. Currently supported for `centiloid`, `centaur`, `centaurz`, `suvr`, `abetaindex`, `abetaload`, `rigid`, and `adni-pet-core`. When registration is enabled (no `--skip-normalization`), the output directory must be empty to avoid overwriting. |
| `--bids <regex>` | Treat `--input` as a PET-BIDS dataset root, recursively process PET NIfTI files under `pet/` whose BIDS relative path, filename, or basename matches `<regex>`, and write outputs to the `--output` directory using the original BIDS basename plus the command suffix, such as `sub-01_ses-01_pet_rigid_aligned.nii` or `sub-01_ses-01_pet_ADNI_style.nii`. |
| `--iterative` | Use iterative rigid transformation |
| `--manual-fov` | Enable manual field-of-view placement |
| `--skip-normalization` | Skip spatial normalization step |
| `--suvr` | Include SUVr values in metric outputs |
| `--tracer <tracer>` | Tracer type for fill-states metric (`fillstates` command only). Supported values: `fbp`, `fdg`, `ftp`. |
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
