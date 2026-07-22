# dcccpy-linux-arm64-runtime

`dcccpy-linux-arm64-runtime` carries the prebuilt Linux aarch64 `DCCCcore`
runtime used by `dcccpy[linux-runtime]` and `dcccpy[runtime]` on ARM64 Linux,
including NVIDIA DGX Spark.

Before building this package, vendor the release asset:

```bash
python scripts/vendor_dccccore.py --version 4.2.3 --release-platform ubuntu-latest-arm64
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
