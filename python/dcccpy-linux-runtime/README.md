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

The vendored runtime is intentionally ignored by git because it contains large
ONNX model and NIfTI template assets.
