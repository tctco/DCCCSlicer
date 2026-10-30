# dcccpy

### Native PET segmentation

```python
import dcccpy
result = dcccpy.seg("pet.nii.gz", "labels.nii.gz")
labels = result.load_output()
```

Requires a DCCCcore build containing the `seg` command and its bundled model.
The signed integer output matches the input PET's native shape and geometry.
The default `mode="freesurfer"` returns raw DKT/aseg IDs; `mode="training"`
returns IDs 0–95. `mode="neurosegmenter"` provides partial regional compatibility:
brainstem/pons IDs 45/46 are unavailable, so pons SUVR is unsupported.
The bound no-flip-trained model always runs one unflipped prediction. There is
no TTA/flip option; legacy calls passing `tta=` raise `TypeError`.
Every `seg()` call requires matching DCCCcore `4.6.2-alpha`, including explicit
`config=` or `bundle=` calls; older native versions are rejected before segmentation.
Use `threads=4` to select the CPU thread count, `bundle=...` to override the model
directory, and `debug=True` to save intermediate grids and a transform receipt.
The approximately 62.4 MB segmentation model is excluded from PyPI wheels.
With a slim runtime, the first `seg()` call downloads the separately published
`DCCCseg-4.6.2-alpha.zip` Release asset, verifies the model SHA-256 and caches it
under the dcccpy cache directory. A full native installation reuses its bundled
model after checking its bound checkpoint hash. An older incompatible native
checkpoint is rejected rather than silently reused. Existing
`DCCCPY_AUTO_DOWNLOAD=0` disables implicit downloads; provision
offline use with `dcccpy.download_seg_model()` or pass `bundle=` pointing to
the extracted `seg` directory. `DCCCPY_SEG_MODEL_URL` selects a mirror while
retaining checksum verification.
An explicit `config=` delegates model selection to that configuration and
suppresses default-model provisioning. With a slim runtime and a configuration
that only changes other settings, also pass `bundle=dcccpy.download_seg_model()`.
An optional `rigid_header=...` must retain the exact native PET values and record
exactly two rigid steps. Existing output images are refused by the core.

`dcccpy` is a Python wrapper for the `DCCCcore` command-line tool from
DCCCSlicer. It keeps the native C++ core as the execution engine and provides a
small Python API for running common PET biomarker workflows.

## Install

The default package is slim:

```bash
pip install dcccpy
```

The first call downloads the matching `DCCCcore` release package into the local
user cache if no native runtime is otherwise available. The slim package also
installs `nibabel` for image loading and nibabel-style image inputs.

The downloaded GitHub release is the full `DCCCcore` runtime. The optional PyPI
runtime wheels are smaller and omit the `fast_and_acc` registration model/config
and ADAD decoupler ONNX ensemble.

Users who prefer `pip install` to include the native runtime and avoid first-run
download can use the platform-selecting runtime extra:

```bash
pip install "dcccpy[runtime]"
```

Platform-specific runtime extras are also available:

```bash
pip install "dcccpy[linux-runtime]"
pip install "dcccpy[windows-runtime]"
pip install "dcccpy[macos-runtime]"
```

The platform-specific extras are guarded by environment markers, so they only
install a runtime wheel on the matching platform. Linux x86_64 uses
`dcccpy-linux-runtime`; Linux aarch64 (including NVIDIA DGX Spark) uses
`dcccpy-linux-arm64-runtime`.

## Python API

```python
import dcccpy

result = dcccpy.centiloid("amyloid_pet.nii", skip_normalization=True)
print(result.metrics)
print(result.output)
```

The `output` argument is optional for single-image Python calls. When omitted,
`dcccpy` creates a temporary output NIfTI path and returns it in the result.
Relative input and output paths are resolved from the current Python working
directory before `DCCCcore` is invoked.

```python
result = dcccpy.centiloid("amyloid_pet.nii")
print(result.output)           # temporary output path
print(result.metrics.get("fbp"))
```

The wrapper accepts nibabel-style image objects with `to_filename()`:

```python
import nibabel as nib
import dcccpy

image = nib.load("amyloid_pet.nii.gz")
result = dcccpy.centiloid(image, skip_normalization=True)
output_image = result.load_output()
```

Common helpers mirror `DCCCcore` subcommands:

```python
dcccpy.centiloid("amyloid.nii", suvr=True)
dcccpy.centaurz("tau.nii", report_detailed_regions=True)
dcccpy.fillstates("fdg.nii", tracer="fdg")
dcccpy.adni_pet_core("fdg.nii", tracer="fdg")
dcccpy.normalize("pet.nii", iterative=True)
dcccpy.pet_motion_correct(
    "dynamic_pet.nii.gz",
    "averaged_pet.nii.gz",
    save_corrected_dynamic="corrected.nii.gz",
    motion_output="motion.tsv",
)
dcccpy.run(["centiloid", "--input", "a.nii", "--output", "b.nii"])
```

Each helper returns `DCCCResult` with:

- `returncode`
- `stdout` / `stderr`
- `output`
- `metrics`, parsed from numeric lines in stdout
- `load_output()`, which loads the output with nibabel

## Command Line

`dcccpy` also forwards raw arguments to `DCCCcore`:

```bash
dcccpy --help
dcccpy centiloid --input amyloid_pet.nii --output result.nii
dcccpy adni-pet-core --input fdg.nii --output fdg_adni.nii --tracer fdg
```

The Python helper accepts the same staged exports. Pass one integer or a
sequence; only the requested stages are saved:

```python
dcccpy.adni_pet_core(
    "dynamic_pet.nii.gz",
    "standardized.nii.gz",
    tracer="fdg",
    level=(1, 3),
    deface=True,
)
```

Level 1 is the motion-corrected dynamic PET, Level 2 is its 3D average, and
Level 3 is the standardized ADNI-style image. The output argument names the
highest selected level, while selected lower levels receive `_Coreg` or
`_Coreg_Avg` suffixes.

Set `deface=True` to zero facial voxels in every selected output. Motion and
spatial transforms are estimated from the original PET before the privacy mask
is applied, so the flag does not change the estimated alignment.

`adni_pet_core` deliberately has no default tracer. Use `abeta`, `tau`, `fdg`,
or `dat`; FDG selects SCAN-inspired top-50% pons/vermis reference normalization,
DAT selects PPMI-style occipital-reference normalization, and Aβ/tau retain
cerebellar gray normalization.

The bundled FDG reference combines Clinica pons and AAL2 vermis masks; it is
an approximation of the SCAN reference. Rigid alignment is unchanged. See the repository's
[ADNI FDG compatibility note](../../docs/adni-fdg-preprocessing-validation.md).

## Runtime lookup

At runtime, `dcccpy` looks for `DCCCcore` in this order:

1. `DCCCPY_DCCCCORE` environment variable.
2. A vendored binary inside the installed `dcccpy` wheel.
3. A binary from `dcccpy-linux-runtime`, `dcccpy-linux-arm64-runtime`,
   `dcccpy-windows-runtime`, or `dcccpy-macos-runtime`, installed by
   `dcccpy[runtime]` or the platform-specific runtime extras.
4. The local dcccpy cache populated by automatic download.
5. `DCCCcore` on `PATH`.
6. Automatic download from GitHub releases, unless disabled.

Useful environment variables:

- `DCCCPY_DCCCCORE`: exact path to a `DCCCcore` executable.
- `DCCCPY_AUTO_DOWNLOAD=0`: disable first-run automatic download.
- `DCCCPY_CACHE_DIR`: override the runtime cache directory.
- `DCCCPY_RELEASE_REPO`: override the GitHub release repository.
- `DCCCPY_DCCCCORE_URL`: override the release asset URL.

## macOS Gatekeeper

On macOS, downloaded command-line executables may be blocked by Gatekeeper until
the user explicitly allows them. If `DCCCcore` fails to start with an error such
as "Operation not permitted" or "developer cannot be verified", `dcccpy` prints
a hint with the exact runtime path.

You can allow the runtime from System Settings > Privacy & Security, or remove
the quarantine flag in Terminal:

```bash
xattr -dr com.apple.quarantine /path/to/DCCCcore
```

Run the same command for the path shown in the `dcccpy` error message, then run
the calculation again.

## Runtime Packaging

Release wheels should vendor the matching `DCCCcore` runtime tree before build:

```bash
python scripts/vendor_dccccore.py --version 4.6.2-alpha --release-platform ubuntu-latest-x64
python -m build --wheel
```

The Linux ARM64 runtime package uses the corresponding release asset:

```bash
cd python/dcccpy-linux-arm64-runtime
python scripts/vendor_dccccore.py --version 4.6.2-alpha --release-platform ubuntu-latest-arm64
python -m build --wheel
```

The source tree intentionally does not commit the vendored runtime because it
contains large ONNX model and NIfTI runtime assets.

The preferred distribution layout is:

- `dcccpy`: slim Python package with nibabel; downloads runtime on first use.
- `dcccpy-linux-runtime`: optional Linux runtime wheel.
- `dcccpy-linux-arm64-runtime`: optional Linux aarch64 runtime wheel.
- `dcccpy-windows-runtime`: optional Windows runtime wheel.
- `dcccpy-macos-runtime`: optional macOS arm64 runtime wheel.
- `dcccpy[runtime]`: installs the matching runtime package on supported platforms.

## Packaging note

The current optional runtime wheels use a PyPI-size profile for DCCCcore version 4.6.2-alpha. They omit the
`fast_and_acc` registration model/config and the ADAD decoupler ONNX ensemble,
while keeping the default spatial normalization model and assets needed by
common Centiloid/CenTauR/CenTauRz workflows.

Each runtime package declares its bundled DCCCcore version. The wrapper ignores
an installed runtime whose native version does not match, preventing an older
runtime package from shadowing the requested release.
