# Publishing dcccpy to PyPI

GitHub Actions publishes the platform-specific `DCCCcore` archives. Python
packages are built and uploaded separately so they can be inspected before
PyPI publication.

## Prerequisites

- A completed GitHub release such as `v4.2.4`
- `/home/cheng/.pypirc.pypi` for PyPI
- `/home/cheng/.pypirc.pypi.test` for TestPyPI
- `uv`, `gh`, and enough disk space for the runtime archives

Keep the `dcccpy` runtime dependency pins and each runtime package version in
sync before building.

## Build

Download the required `DCCCcore` archives:

```bash
mkdir -p /tmp/dcccpy-release
gh release download v4.2.4 \
  --dir /tmp/dcccpy-release \
  --pattern 'DCCCcore-4.2.4-*.zip'
```

Build the slim package and the required runtime wheels into a clean directory.
For example, Linux ARM64:

```bash
rm -rf /tmp/dcccpy-dist
mkdir -p /tmp/dcccpy-dist

uvx --from build pyproject-build \
  --outdir /tmp/dcccpy-dist \
  python/dcccpy

python python/dcccpy-linux-arm64-runtime/scripts/vendor_dccccore.py \
  --version 4.2.4 \
  --archive /tmp/dcccpy-release/DCCCcore-4.2.4-ubuntu-latest-arm64.zip \
  --profile pypi-slim \
  --force
uvx --from build pyproject-build \
  --wheel \
  --outdir /tmp/dcccpy-dist \
  python/dcccpy-linux-arm64-runtime
```

The other runtime directories provide the same `vendor_dccccore.py` workflow.
Always use `--profile pypi-slim` for PyPI runtime wheels.

## Validate And Upload

Validate every distribution before uploading:

```bash
uvx --from twine twine check /tmp/dcccpy-dist/*
```

Upload to TestPyPI when a platform package needs installation testing:

```bash
uvx --from twine twine upload \
  --config-file /home/cheng/.pypirc.pypi.test \
  --repository testpypi \
  /tmp/dcccpy-dist/*
```

Upload the verified, previously unpublished files to PyPI:

```bash
uvx --from twine twine upload \
  --config-file /home/cheng/.pypirc.pypi \
  --repository pypi \
  /tmp/dcccpy-dist/*
```

PyPI does not allow replacing a file for an existing version. Check the project
JSON endpoints first and upload only missing versions or files.

## Platform Verification

Verify dependency resolution on the target architecture. Linux ARM64 can be
tested from an x86_64 host with Docker and QEMU:

```bash
docker run --rm --platform linux/arm64 python:3.11-slim sh -lc '
  python -m pip install --no-cache-dir "dcccpy[runtime]"
  python -c "import dcccpy; print(dcccpy.run([\"--version\"]).stdout)"
'
```

The test must install the matching runtime wheel and print the expected
`DCCCcore` release version.
