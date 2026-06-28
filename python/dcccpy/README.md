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
user cache if no native runtime is otherwise available.

For Linux users who prefer `pip install` to include the native runtime and avoid
first-run download:

```bash
pip install "dcccpy[linux-runtime]"
```

For nibabel convenience helpers:

```bash
pip install "dcccpy[nibabel]"
```

Both extras can be combined:

```bash
pip install "dcccpy[linux-runtime,nibabel]"
```

## Python API

```python
import dcccpy

result = dcccpy.centiloid("amyloid_pet.nii", skip_normalization=True)
print(result.metrics)
print(result.output)
```

The `output` argument is optional for single-image Python calls. When omitted,
`dcccpy` creates a temporary output NIfTI path and returns it in the result.

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
3. A binary from `dcccpy-linux-runtime`, installed by `dcccpy[linux-runtime]`.
4. The local dcccpy cache populated by automatic download.
5. `DCCCcore` on `PATH`.
6. Automatic download from GitHub releases, unless disabled.

Useful environment variables:

- `DCCCPY_DCCCCORE`: exact path to a `DCCCcore` executable.
- `DCCCPY_AUTO_DOWNLOAD=0`: disable first-run automatic download.
- `DCCCPY_CACHE_DIR`: override the runtime cache directory.
- `DCCCPY_RELEASE_REPO`: override the GitHub release repository.
- `DCCCPY_DCCCCORE_URL`: override the release asset URL.

## Runtime Packaging

Release wheels should vendor the matching `DCCCcore` runtime tree before build:

```bash
python scripts/vendor_dccccore.py --version 4.2.3 --release-platform ubuntu-latest-x64
python -m build --wheel
```

The source tree intentionally does not commit the vendored runtime because it
contains large ONNX model and NIfTI runtime assets.

The preferred distribution layout is:

- `dcccpy`: slim Python package; downloads runtime on first use.
- `dcccpy-linux-runtime`: optional Linux runtime wheel.
- `dcccpy[linux-runtime]`: installs both packages.

## Packaging note

The Linux `DCCCcore-4.2.3-ubuntu-latest-x64.zip` release asset contains the
native executable, `libtbb`, ONNX registration/decoupling models, configuration
files, and NIfTI template/mask assets. That full runtime is the correct unit to
vendor for a wheel that works immediately after `pip install dcccpy`.

The full Linux runtime wheel is about 167 MB for version 4.2.3, so a public
PyPI upload may require a file size limit increase unless a future release
splits a smaller runtime profile from the full `DCCCcore` package.
