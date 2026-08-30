# dcccpy-macos-runtime

`dcccpy-macos-runtime` carries the prebuilt macOS arm64 `DCCCcore` runtime used
by `dcccpy[macos-runtime]` and by `dcccpy[runtime]` on Apple Silicon Macs.

This package is optional. The plain `dcccpy` package can download `DCCCcore`
from GitHub releases on first use when no installed runtime is available.

Before building this package, vendor the release asset:

```bash
python scripts/vendor_dccccore.py --version 4.3.0 --release-platform macos-latest-arm64
python -m build --wheel
```

For a PyPI-size runtime wheel, use the slim profile:

```bash
python scripts/vendor_dccccore.py --profile pypi-slim --force
python -m build --wheel
```
