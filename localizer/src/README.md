# Deep Cascaded Cerebral Calculator Core

A deep learning-based multimodal brain PET image standardization and semi-quantitative analysis software for neuroimaging biomarker calculation.

## Overview

The Deep Cascaded Cerebral Calculator Core is a comprehensive C++ toolkit designed for automated quantitative analysis of brain PET images. It provides standardized spatial normalization and calculates various semi-quantitative metrics including Centiloid, CenTauR, CenTauRz, and custom SUVr values. The software employs deep learning models for accurate spatial registration and supports multiple PET tracers for both amyloid and tau imaging.

### Key Features

- **Multi-biomarker Support**: Calculates Centiloid (amyloid), CenTauR/CenTauRz (tau), fill-states, and custom SUVr metrics
- **Deep Learning Pipeline**: Utilizes ONNX-based neural networks for spatial normalization
- **Modular Architecture**: Extensible design with clean interfaces for adding new biomarkers
- **Multi-tracer Compatibility**: Supports various PET tracers with tracer-specific calibrations
- **Decoupling Analysis**: Advanced pathology-specific signal extraction
- **Configuration-driven**: Flexible TOML-based configuration system

## Usage

The software provides a command-line interface with multiple subcommands for different analysis tasks.

### Basic Commands

#### Centiloid Analysis
Calculate standardized amyloid burden scores:

```bash
# Basic Centiloid calculation
./CentiloidCalculator centiloid --input amyloid_pet.nii --output result.nii

# With configuration file
./CentiloidCalculator centiloid --input amyloid_pet.nii --output result.nii --config custom_config.toml

# Include SUVr values in output
./CentiloidCalculator centiloid --input amyloid_pet.nii --output result.nii --suvr

# Skip spatial normalization (pre-normalized images)
./CentiloidCalculator centiloid --input normalized_pet.nii --output result.nii --skip-normalization
```

Batch processing of multiple amyloid PET images:

```bash
# Process all .nii / .nii.gz files under input_dir and write outputs to output_dir
./CentiloidCalculator centiloid --input input_dir --output output_dir --batch
```

#### CenTauR Analysis
Calculate standardized tau burden scores:

```bash
# CenTauR percentile scale
./CentiloidCalculator centaur --input tau_pet.nii --output result.nii

# CenTauRz z-score scale
./CentiloidCalculator centaurz --input tau_pet.nii --output result.nii
```

#### Fill-states Analysis
Calculate fill-states metric based on voxel-wise z-scores within tracer-specific meta-ROIs:

```bash
# Amyloid tracer (FBP) fill-states
./CentiloidCalculator fillstates --input amyloid_pet.nii --output result.nii --tracer fbp

# FDG neurodegeneration fill-states
./CentiloidCalculator fillstates --input fdg_pet.nii --output result.nii --tracer fdg

# FTP tau fill-states
./CentiloidCalculator fillstates --input ftp_pet.nii --output result.nii --tracer ftp
```

The command produces:

- A **normalized output** image at the path specified by `--output`, same as other SUVr-derived metrics.
- A **fill_states_map** image saved alongside the output, using the same base name with suffix `_fill_states_map`, containing a 0/1 mask of voxels exceeding the z-score threshold within the meta-ROI.

#### Custom SUVr Calculation
Calculate SUVr with user-defined regions:

```bash
./CentiloidCalculator suvr --input pet.nii --output result.nii \
  --voi-mask target_region.nii --ref-mask reference_region.nii
```

Batch SUVr calculation with user-defined regions:

```bash
./CentiloidCalculator suvr --input input_dir --output output_dir --batch \
  --voi-mask target_region.nii --ref-mask reference_region.nii
```

#### Spatial Normalization Only
Perform spatial standardization without metric calculation:

```bash
# Standard normalization
./CentiloidCalculator normalize --input pet.nii --output normalized.nii

# ADNI-style processing
./CentiloidCalculator normalize --input pet.nii --output normalized.nii --ADNI-PET-core

# Iterative rigid registration
./CentiloidCalculator normalize --input pet.nii --output normalized.nii --iterative
```

#### Decoupling Analysis
Extract pathology-specific signals:

```bash
# Amyloid decoupling
./CentiloidCalculator decouple --input pet.nii --output decoupled.nii --modality abeta

# Tau decoupling
./CentiloidCalculator decouple --input pet.nii --output decoupled.nii --modality tau
```

### Command Options

| Option | Description |
|--------|-------------|
| `--config <file>` | Configuration file path (default: config.toml) |
| `--debug` | Enable debug mode with intermediate outputs |
| `--batch` | Enable batch processing mode. In batch mode, `--input` and `--output` are treated as directories, all `.nii` / `.nii.gz` files in the input directory are processed, and outputs are written as `<filename>_processed.nii` together with `results.csv` and `batch_info.txt` in the output directory. Currently supported for `centiloid`, `centaur`, `centaurz`, and `suvr` commands. When registration is enabled (no `--skip-normalization`), the output directory must be empty to avoid overwriting. |
| `--iterative` | Use iterative rigid transformation |
| `--manual-fov` | Enable manual field-of-view placement |
| `--skip-normalization` | Skip spatial normalization step |
| `--suvr` | Include SUVr values in metric outputs |
| `--tracer <tracer>` | Tracer type for fill-states metric (`fillstates` command only). Supported values: `fbp`, `fdg`, `ftp`. Required for `fillstates`. |

## Developers

To add your own brain PET metric, please refer to the [developer guide](../../docs/developer_guide.md).