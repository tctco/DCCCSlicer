#!/usr/bin/env python3
"""Vendor a Windows DCCCcore release asset into dcccpy-windows-runtime."""

from __future__ import annotations

import argparse
import shutil
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path


REPO = "tctco/DCCCSlicer"
DEFAULT_VERSION = "4.2.3"
DEFAULT_RELEASE_PLATFORM = "windows-latest-x64"

EXCLUDE_PROFILES = {
    "full": (),
    "no-fast-and-acc": (
        "assets/configs/config.fast_and_acc.toml",
        "assets/models/registration/affine_voxelmorph.fast_and_acc.onnx",
    ),
    "pypi-slim": (
        "assets/configs/config.fast_and_acc.toml",
        "assets/models/registration/affine_voxelmorph.fast_and_acc.onnx",
        "assets/models/decouple/",
    ),
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", default=DEFAULT_VERSION, help="DCCCSlicer release version without leading v")
    parser.add_argument("--release-platform", default=DEFAULT_RELEASE_PLATFORM, help="Release asset platform suffix")
    parser.add_argument("--repo", default=REPO, help="GitHub repository in owner/name form")
    parser.add_argument(
        "--profile",
        choices=sorted(EXCLUDE_PROFILES),
        default="full",
        help="Runtime asset profile to vendor",
    )
    parser.add_argument("--archive", type=Path, default=None, help="Use an existing release zip instead of downloading")
    parser.add_argument("--force", action="store_true", help="Replace an existing vendor directory")
    return parser.parse_args()


def download(url: str, dest: Path) -> None:
    with urllib.request.urlopen(url) as response, dest.open("wb") as handle:
        shutil.copyfileobj(response, handle)


def relative_runtime_path(path: Path) -> str:
    parts = path.parts
    if len(parts) <= 1:
        return path.name
    return Path(*parts[1:]).as_posix()


def excluded(path: Path, prefixes: tuple[str, ...]) -> bool:
    rel = relative_runtime_path(path)
    return any(rel == prefix or rel.startswith(prefix) for prefix in prefixes)


def prune_profile(dest: Path, profile: str) -> None:
    prefixes = EXCLUDE_PROFILES[profile]
    if not prefixes:
        return

    removed = 0
    for path in sorted(dest.rglob("*"), reverse=True):
        rel_path = path.relative_to(dest.parent)
        if excluded(rel_path, prefixes):
            if path.is_dir():
                shutil.rmtree(path)
            else:
                path.unlink()
            removed += 1

    for path in sorted(dest.rglob("*"), reverse=True):
        if path.is_dir() and not any(path.iterdir()):
            path.rmdir()

    print(f"Pruned {removed} paths for profile {profile}", file=sys.stderr)


def main() -> int:
    args = parse_args()
    project_root = Path(__file__).resolve().parents[1]
    dest = project_root / "src" / "dcccpy_windows_runtime" / "vendor" / "dccccore" / "windows-x86_64"
    asset_name = f"DCCCcore-{args.version}-{args.release_platform}.zip"
    url = f"https://github.com/{args.repo}/releases/download/v{args.version}/{asset_name}"

    if dest.exists():
        if not args.force:
            print(f"{dest} already exists. Use --force to replace it.", file=sys.stderr)
            return 2
        shutil.rmtree(dest)

    with tempfile.TemporaryDirectory(prefix="dcccpy-windows-runtime-") as temp_name:
        temp = Path(temp_name)
        if args.archive is None:
            archive = temp / asset_name
            print(f"Downloading {url}", file=sys.stderr)
            download(url, archive)
        else:
            archive = args.archive.expanduser().resolve()
            print(f"Using local archive {archive}", file=sys.stderr)

        extract_dir = temp / "extract"
        extract_dir.mkdir()
        with zipfile.ZipFile(archive) as zf:
            zf.extractall(extract_dir)

        roots = [path for path in extract_dir.iterdir() if path.is_dir()]
        if len(roots) != 1:
            print(f"Expected exactly one top-level directory in {archive}, found {len(roots)}.", file=sys.stderr)
            return 3

        shutil.copytree(roots[0], dest)

    prune_profile(dest, args.profile)

    print(f"Vendored {asset_name} into {dest} using profile {args.profile}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
