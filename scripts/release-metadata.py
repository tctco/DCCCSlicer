#!/usr/bin/env python3
"""Validate tag/dispatch version against all committed core version files."""
from __future__ import annotations
import os
from pathlib import Path
import re


def release_version(cmake: str, *, requested: str = "", tag: str = "") -> str:
    project = re.search(r"project\(DCCCcore VERSION (\d+\.\d+\.\d+)\)", cmake)
    suffix = re.search(r'set\(DCCCCORE_VERSION_SUFFIX "([^"]*)"\)', cmake)
    if not project or not suffix:
        raise ValueError("Cannot resolve DCCCcore source version")
    declared = project[1] + suffix[1]
    for value in (requested, tag.removeprefix("v")):
        if value and value != declared:
            raise ValueError(f"Release version {value!r} disagrees with source {declared!r}")
    if not re.fullmatch(r"\d+\.\d+\.\d+(?:-[A-Za-z0-9.]+)?", declared):
        raise ValueError("Invalid source release version")
    return declared


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    source = root / "localizer/src"
    tag = os.environ.get("GITHUB_REF_NAME", "") if os.environ.get("GITHUB_REF", "").startswith("refs/tags/") else ""
    version = release_version((source / "CMakeLists.txt").read_text(),
                              requested=os.environ.get("REQUESTED_VERSION", ""), tag=tag)
    conan = re.search(r'version\s*=\s*"([^"]+)"', (source / "conanfile.py").read_text())
    header = re.search(r'SOFTWARE_VERSION\s*=\s*"([^"]+)"', (source / "core/config/Version.h").read_text())
    if not conan or not header or conan[1] != version or header[1] != version:
        raise SystemExit("CMake, Conan and Version.h versions are inconsistent")
    print(version)
    if os.environ.get("GITHUB_ENV"):
        with open(os.environ["GITHUB_ENV"], "a", encoding="utf-8") as handle:
            handle.write(f"PACKAGE_VERSION={version}\n")
