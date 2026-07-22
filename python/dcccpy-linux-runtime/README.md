# dcccpy-linux-runtime

`dcccpy-linux-runtime` carries the prebuilt Linux `DCCCcore` runtime used by
`dcccpy[linux-runtime]`.

This package is optional. The plain `dcccpy` package can download `DCCCcore`
from GitHub releases on first use. Install this runtime package when a user or
environment needs `pip install` to provide the native runtime without first-run
download.

Before building this package, vendor the release asset:

```bash
python scripts/vendor_dccccore.py --version 4.2.3 --release-platform ubuntu-latest-x64
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
