"""Resolve the bundled DCCCcore executable path."""

import sys
from pathlib import Path


def dccccore_executable_path(plugin_path, platform=None):
    """Return the platform-specific DCCCcore executable bundled with the UI."""

    platform = platform or sys.platform
    executable_name = "DCCCcore.exe" if platform == "win32" else "DCCCcore"
    return Path(plugin_path) / "cpp" / executable_name
