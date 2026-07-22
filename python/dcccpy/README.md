# dcccpy

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
dcccpy.normalize("pet.nii", iterative=True)
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
```

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
python scripts/vendor_dccccore.py --version 4.2.4 --release-platform ubuntu-latest-x64
python -m build --wheel
```

The Linux ARM64 runtime package uses the corresponding release asset:

```bash
cd python/dcccpy-linux-arm64-runtime
python scripts/vendor_dccccore.py --version 4.2.4 --release-platform ubuntu-latest-arm64
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

The runtime wheels use a PyPI-size profile for version 4.2.3. They omit the
`fast_and_acc` registration model/config and the ADAD decoupler ONNX ensemble,
while keeping the default spatial normalization model and assets needed by
common Centiloid/CenTauR/CenTauRz workflows.

The plain `dcccpy` package remains slim and downloads the full matching
`DCCCcore` release package from GitHub on first use when no installed runtime is
available.
