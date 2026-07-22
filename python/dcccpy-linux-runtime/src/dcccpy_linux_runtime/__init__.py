from __future__ import annotations

from pathlib import Path


def dccccore_root() -> Path:
    return Path(__file__).resolve().parent / "vendor" / "dccccore" / "linux-x86_64"


def dccccore_path() -> Path:
    return dccccore_root() / "DCCCcore"


__all__ = ["dccccore_path", "dccccore_root"]
