from __future__ import annotations

from pathlib import Path

DCCCCORE_VERSION = "4.5.1-alpha"


def dccccore_root() -> Path:
    return Path(__file__).resolve().parent / "vendor" / "dccccore" / "macos-arm64"


def dccccore_path() -> Path:
    return dccccore_root() / "DCCCcore"


__all__ = ["DCCCCORE_VERSION", "dccccore_path", "dccccore_root"]
