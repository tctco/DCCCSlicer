"""macOS Gatekeeper guidance for bundled DCCCcore launches."""

from __future__ import annotations

import os
import shlex
import sys
from pathlib import Path


_SECURITY_MARKERS = (
    "cannot be opened because the developer cannot be verified",
    "developer cannot be verified",
    "operation not permitted",
    "permission denied",
    "damaged and can't be opened",
    "damaged and cannot be opened",
    "malware",
    "code signature",
    "not signed",
    "quarantine",
)


def is_macos() -> bool:
    return sys.platform == "darwin"


def macos_security_hint(executable_path: str | os.PathLike[str]) -> str:
    path = Path(executable_path).expanduser()
    quoted_path = shlex.quote(os.fspath(path))
    return (
        "macOS may have blocked DCCCcore because it was downloaded from the internet.\n"
        "Open System Settings > Privacy & Security and allow DCCCcore, or run this "
        "Terminal command once:\n"
        f"  xattr -dr com.apple.quarantine {quoted_path}\n"
        "Then run the calculation again."
    )


def looks_like_macos_security_block(
    message: str = "",
    *,
    returncode: int | None = None,
) -> bool:
    if not is_macos():
        return False

    text = message.lower()
    if any(marker in text for marker in _SECURITY_MARKERS):
        return True

    return returncode in {126, -9}


def append_macos_security_hint(
    message: str,
    executable_path: str | os.PathLike[str],
    *,
    returncode: int | None = None,
    force: bool = False,
) -> str:
    if "com.apple.quarantine" in message:
        return message

    if not force and not looks_like_macos_security_block(message, returncode=returncode):
        return message

    if not is_macos():
        return message

    hint = macos_security_hint(executable_path)
    return f"{message.rstrip()}\n\n{hint}" if message.strip() else hint
