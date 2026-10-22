#!/usr/bin/env python3
"""Assemble the platform-independent DCCC Seg Release asset."""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import zipfile


MODEL_FILES = ("dccc_seg.onnx", "manifest.json", "grid.json", "plans.json", "dataset.json",
               "labels.json", "neurosegmenter_mapping.json", "validation.json")


def package_model(bundle: Path, destination: Path, version: str) -> Path:
    if not re.fullmatch(r"\d+\.\d+\.\d+(?:-[A-Za-z0-9.]+)?", version):
        raise ValueError("Invalid release version")
    for name in MODEL_FILES:
        if not (bundle / name).is_file():
            raise FileNotFoundError(bundle / name)
    destination.mkdir(parents=True, exist_ok=True)
    archive = destination / f"DCCCseg-{version}.zip"
    with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as output:
        for name in (*MODEL_FILES, "README.md", "QUICKSTART.md", "dccc_seg.py",
                     "test_bundle.py", "export_onnx.py", "validate_pilot.py"):
            source = bundle / name
            if source.is_file():
                output.write(source, "seg/" + name)
    return archive


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle", type=Path, default=Path("localizer/src/assets/models/seg"))
    parser.add_argument("--destination", type=Path, default=Path("dist"))
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    print(package_model(args.bundle, args.destination, args.version))
