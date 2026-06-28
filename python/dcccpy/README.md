# dcccpy

`dcccpy` is a Python wrapper for the `DCCCcore` command-line tool from
DCCCSlicer. It keeps the native C++ core as the execution engine and provides a
small Python API for running common PET biomarker workflows.

```python
import dcccpy

result = dcccpy.centiloid("amyloid_pet.nii", skip_normalization=True)
print(result.metrics)
print(result.output)
```

The `output` argument is optional for single-image Python calls. When omitted,
`dcccpy` creates a temporary output NIfTI path and returns it in the result.

## Runtime lookup

At runtime, `dcccpy` looks for `DCCCcore` in this order:

1. `DCCCPY_DCCCCORE` environment variable.
2. A vendored binary inside the installed wheel.
3. `DCCCcore` on `PATH`.

Release wheels should vendor the matching `DCCCcore` runtime tree before build:

```bash
python scripts/vendor_dccccore.py --version 4.2.3 --release-platform ubuntu-latest-x64
python -m build --wheel
```

The vendored tree is intentionally not committed to git because it contains
large ONNX model and NIfTI runtime assets.

## Packaging note

The Linux `DCCCcore-4.2.3-ubuntu-latest-x64.zip` release asset contains the
native executable, `libtbb`, ONNX registration/decoupling models, configuration
files, and NIfTI template/mask assets. That full runtime is the correct unit to
vendor for a wheel that works immediately after `pip install dcccpy`.

The complete Linux runtime is large, so a public PyPI upload may require a file
size limit increase unless a future release splits a smaller runtime profile
from the full `DCCCcore` package.
