#!/usr/bin/env python3
"""Vendor a DCCCcore release asset into the dcccpy package tree."""

from __future__ import annotations

import argparse
import os
import shutil
import stat
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path


REPO = "tctco/DCCCSlicer"
DEFAULT_VERSION = "4.2.3"
DEFAULT_RELEASE_PLATFORM = "ubuntu-latest-x64"
DEFAULT_VENDOR_PLATFORM = "linux-x86_64"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", default=DEFAULT_VERSION, help="DCCCSlicer release version without leading v")
    parser.add_argument("--release-platform", default=DEFAULT_RELEASE_PLATFORM, help="Release asset platform suffix")
    parser.add_argument("--vendor-platform", default=DEFAULT_VENDOR_PLATFORM, help="dcccpy vendor platform directory")
    parser.add_argument("--repo", default=REPO, help="GitHub repository in owner/name form")
    parser.add_argument("--dest", type=Path, default=None, help="Override destination vendor directory")
    parser.add_argument("--force", action="store_true", help="Replace an existing vendor directory")
    return parser.parse_args()


def download(url: str, dest: Path) -> None:
    with urllib.request.urlopen(url) as response, dest.open("wb") as handle:
        shutil.copyfileobj(response, handle)


def main() -> int:
    args = parse_args()
    project_root = Path(__file__).resolve().parents[1]
    dest = args.dest or project_root / "src" / "dcccpy" / "vendor" / "dccccore" / args.vendor_platform
    asset_name = f"DCCCcore-{args.version}-{args.release_platform}.zip"
    url = f"https://github.com/{args.repo}/releases/download/v{args.version}/{asset_name}"

    if dest.exists():
        if not args.force:
            print(f"{dest} already exists. Use --force to replace it.", file=sys.stderr)
            return 2
        shutil.rmtree(dest)

    with tempfile.TemporaryDirectory(prefix="dcccpy-vendor-") as temp_name:
        temp = Path(temp_name)
        archive = temp / asset_name
        print(f"Downloading {url}", file=sys.stderr)
        download(url, archive)

        extract_dir = temp / "extract"
        extract_dir.mkdir()
        with zipfile.ZipFile(archive) as zf:
            zf.extractall(extract_dir)

        roots = [path for path in extract_dir.iterdir() if path.is_dir()]
        if len(roots) != 1:
            print(f"Expected exactly one top-level directory in {archive}, found {len(roots)}.", file=sys.stderr)
            return 3

        shutil.copytree(roots[0], dest)

    exe = dest / ("DCCCcore.exe" if os.name == "nt" else "DCCCcore")
    if exe.exists() and os.name != "nt":
        mode = exe.stat().st_mode
        exe.chmod(mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

    print(f"Vendored {asset_name} into {dest}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
