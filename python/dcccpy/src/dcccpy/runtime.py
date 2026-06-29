from __future__ import annotations

import os
import platform
import shutil
import stat
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path
from typing import Iterable


DCCCCORE_VERSION = "4.2.3"
RELEASE_REPO = "tctco/DCCCSlicer"


class DCCCcoreNotFoundError(FileNotFoundError):
    """Raised when dcccpy cannot locate a DCCCcore executable."""


class DCCCcoreDownloadError(RuntimeError):
    """Raised when automatic DCCCcore download fails."""


def platform_key() -> str:
    machine = platform.machine().lower()
    if machine in {"x86_64", "amd64"}:
        arch = "x86_64"
    elif machine in {"aarch64", "arm64"}:
        arch = "arm64"
    else:
        arch = machine

    if sys.platform.startswith("linux"):
        return f"linux-{arch}"
    if sys.platform == "darwin":
        return f"macos-{arch}"
    if sys.platform.startswith("win"):
        return f"windows-{arch}"
    return f"{sys.platform}-{arch}"


def release_platform(key: str | None = None) -> str:
    key = key or platform_key()
    mapping = {
        "linux-x86_64": "ubuntu-latest-x64",
        "windows-x86_64": "windows-latest-x64",
        "macos-arm64": "macos-latest-arm64",
    }
    try:
        return mapping[key]
    except KeyError as exc:
        raise DCCCcoreNotFoundError(f"No prebuilt DCCCcore release asset is known for {key}.") from exc


def executable_names() -> tuple[str, ...]:
    return ("DCCCcore.exe", "DCCCcore") if os.name == "nt" else ("DCCCcore",)


def package_root() -> Path:
    return Path(__file__).resolve().parent


def cache_root() -> Path:
    explicit = os.environ.get("DCCCPY_CACHE_DIR")
    if explicit:
        return Path(explicit).expanduser()

    if sys.platform.startswith("win"):
        base = os.environ.get("LOCALAPPDATA")
        if base:
            return Path(base) / "dcccpy"

    base = os.environ.get("XDG_CACHE_HOME")
    if base:
        return Path(base) / "dcccpy"

    return Path.home() / ".cache" / "dcccpy"


def runtime_dir(version: str = DCCCCORE_VERSION, key: str | None = None) -> Path:
    return cache_root() / "dccccore" / version / (key or platform_key())


def candidate_executables(base_dirs: Iterable[Path]) -> list[Path]:
    candidates: list[Path] = []
    for base in base_dirs:
        for name in executable_names():
            candidates.append(base / name)
    return candidates


def _main_package_vendor_dirs() -> list[Path]:
    vendor_root = package_root() / "vendor" / "dccccore"
    key = platform_key()
    return [vendor_root / key, vendor_root]


def _runtime_package_dirs() -> list[Path]:
    roots: list[Path] = []
    try:
        import dcccpy_linux_runtime
    except Exception:
        pass
    else:
        roots.append(Path(dcccpy_linux_runtime.dccccore_root()))

    try:
        import dcccpy_windows_runtime
    except Exception:
        pass
    else:
        roots.append(Path(dcccpy_windows_runtime.dccccore_root()))

    return roots


def _cached_dirs(version: str = DCCCCORE_VERSION) -> list[Path]:
    return [runtime_dir(version)]


def find_existing_dccccore(version: str = DCCCCORE_VERSION) -> Path | None:
    for candidate in candidate_executables(_main_package_vendor_dirs()):
        if candidate.exists():
            return candidate

    for candidate in candidate_executables(_runtime_package_dirs()):
        if candidate.exists():
            return candidate

    for candidate in candidate_executables(_cached_dirs(version)):
        if candidate.exists():
            return candidate

    found = shutil.which("DCCCcore")
    return Path(found) if found else None


def auto_download_enabled() -> bool:
    value = os.environ.get("DCCCPY_AUTO_DOWNLOAD", "1").strip().lower()
    return value not in {"0", "false", "no", "off"}


def release_url(version: str = DCCCCORE_VERSION, repo: str = RELEASE_REPO, platform_name: str | None = None) -> str:
    platform_name = platform_name or release_platform()
    asset = f"DCCCcore-{version}-{platform_name}.zip"
    return f"https://github.com/{repo}/releases/download/v{version}/{asset}"


def _safe_extract(zip_file: zipfile.ZipFile, destination: Path) -> None:
    destination = destination.resolve()
    for member in zip_file.infolist():
        target = (destination / member.filename).resolve()
        if not str(target).startswith(str(destination)):
            raise DCCCcoreDownloadError(f"Unsafe path in DCCCcore archive: {member.filename}")
    zip_file.extractall(destination)


def _copy_runtime_tree(extracted_root: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        shutil.rmtree(destination)
    shutil.copytree(extracted_root, destination)

    for candidate in candidate_executables([destination]):
        if candidate.exists() and os.name != "nt":
            mode = candidate.stat().st_mode
            candidate.chmod(mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def download_dccccore(
    *,
    version: str = DCCCCORE_VERSION,
    repo: str | None = None,
    key: str | None = None,
    force: bool = False,
) -> Path:
    key = key or platform_key()
    destination = runtime_dir(version, key)
    existing = next((path for path in candidate_executables([destination]) if path.exists()), None)
    if existing is not None and not force:
        return existing

    repo = repo or os.environ.get("DCCCPY_RELEASE_REPO", RELEASE_REPO)
    url = os.environ.get("DCCCPY_DCCCCORE_URL") or release_url(
        version=version,
        repo=repo,
        platform_name=release_platform(key),
    )

    with tempfile.TemporaryDirectory(prefix="dcccpy-runtime-") as temp_name:
        temp = Path(temp_name)
        archive = temp / "DCCCcore.zip"
        try:
            with urllib.request.urlopen(url) as response, archive.open("wb") as handle:
                shutil.copyfileobj(response, handle)
        except Exception as exc:
            raise DCCCcoreDownloadError(f"Failed to download DCCCcore from {url}") from exc

        extract_dir = temp / "extract"
        extract_dir.mkdir()
        try:
            with zipfile.ZipFile(archive) as zf:
                _safe_extract(zf, extract_dir)
        except Exception as exc:
            raise DCCCcoreDownloadError(f"Failed to extract DCCCcore archive from {url}") from exc

        roots = [path for path in extract_dir.iterdir() if path.is_dir()]
        if len(roots) != 1:
            raise DCCCcoreDownloadError(f"Expected one top-level directory in DCCCcore archive, found {len(roots)}.")

        _copy_runtime_tree(roots[0], destination)

    existing = next((path for path in candidate_executables([destination]) if path.exists()), None)
    if existing is None:
        raise DCCCcoreDownloadError(f"DCCCcore archive did not contain an executable for {key}.")
    return existing


def dccccore_path(
    executable: str | os.PathLike[str] | None = None,
    *,
    allow_download: bool | None = None,
    version: str = DCCCCORE_VERSION,
) -> Path:
    explicit = executable or os.environ.get("DCCCPY_DCCCCORE")
    if explicit:
        path = Path(explicit).expanduser()
        if path.exists():
            return path
        raise DCCCcoreNotFoundError(f"DCCCcore executable does not exist: {path}")

    existing = find_existing_dccccore(version)
    if existing is not None:
        return existing

    should_download = auto_download_enabled() if allow_download is None else allow_download
    if should_download:
        return download_dccccore(version=version)

    raise DCCCcoreNotFoundError(
        "Could not find DCCCcore. Install dcccpy[runtime], set DCCCPY_DCCCCORE, "
        "put DCCCcore on PATH, or enable automatic download with DCCCPY_AUTO_DOWNLOAD=1."
    )
