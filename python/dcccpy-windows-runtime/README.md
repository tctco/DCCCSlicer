# dcccpy-windows-runtime

`dcccpy-windows-runtime` carries the prebuilt Windows `DCCCcore` runtime used by
`dcccpy[windows-runtime]` and by `dcccpy[runtime]` on Windows.

This package is optional. The plain `dcccpy` package can download `DCCCcore`
from GitHub releases on first use. Install this runtime package when a user or
environment needs `pip install` to provide the native runtime without first-run
download.

Before building this package, vendor the release asset:

```bash
python scripts/vendor_dccccore.py --version 4.2.3 --release-platform windows-latest-x64
python -m build --wheel
```

For a PyPI-size runtime wheel, use the slim profile:

```bash
python scripts/vendor_dccccore.py --profile pypi-slim --force
python -m build --wheel
```

The `pypi-slim` profile omits the `fast_and_acc` registration model/config and
the ADAD decoupler ONNX ensemble. It keeps the default spatial normalization
model and the assets needed by common Centiloid/CenTauR/CenTauRz workflows.

The vendored runtime is intentionally ignored by git because it contains large
ONNX model and NIfTI template assets.
