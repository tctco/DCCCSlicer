"""Optional DCCC Seg model delivery, kept outside PyPI wheels."""
from __future__ import annotations

import hashlib
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import tempfile
import urllib.request
import zipfile

from .runtime import DCCCCORE_VERSION, RELEASE_REPO, auto_download_enabled, cache_root


SEG_MODEL_SHA256 = "8ea46dc8eff44e7a6c55b93ec9a353eb676692741d56a47b04c755a3c956faf6"
MODEL_FILES = ("dccc_seg.onnx", "manifest.json", "grid.json", "plans.json", "dataset.json",
               "labels.json", "neurosegmenter_mapping.json", "validation.json")


class SegModelDownloadError(RuntimeError):
    """The optional segmentation model could not be provisioned."""


def seg_model_dir(version: str = DCCCCORE_VERSION) -> Path:
    if not re.fullmatch(r"\d+\.\d+\.\d+(?:-[A-Za-z0-9.]+)?", version):
        raise ValueError("Invalid segmentation model version")
    return cache_root() / "models" / "seg" / version


def _complete(directory: Path) -> bool:
    return all((directory / name).is_file() for name in MODEL_FILES)


def _verified(directory: Path) -> bool:
    if not _complete(directory):
        return False
    digest = hashlib.sha256()
    with (directory / "dccc_seg.onnx").open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest() == SEG_MODEL_SHA256


def download_seg_model(*, version: str = DCCCCORE_VERSION, force: bool = False) -> Path:
    """Download and verify the optional model; return its cached bundle directory.

    An explicit call provisions offline use even with DCCCPY_AUTO_DOWNLOAD=0.
    DCCCPY_SEG_MODEL_URL can select a mirrored archive; the model SHA-256 is
    still enforced. force=True replaces this version's model cache only.
    """
    destination = seg_model_dir(version)
    if not force and _verified(destination):
        return destination
    if destination.exists() and not force:
        raise SegModelDownloadError("Cached segmentation model is incomplete or corrupt; "
                                    "call download_seg_model(force=True) to repair it")
    repo = os.environ.get("DCCCPY_RELEASE_REPO", RELEASE_REPO)
    url = os.environ.get("DCCCPY_SEG_MODEL_URL") or (
        f"https://github.com/{repo}/releases/download/v{version}/DCCCseg-{version}.zip")
    destination.parent.mkdir(parents=True, exist_ok=True)
    try:
        with tempfile.TemporaryDirectory(prefix=".seg-download-", dir=destination.parent) as temporary:
            temporary = Path(temporary)
            archive = temporary / "model.zip"
            with urllib.request.urlopen(url, timeout=120) as response, archive.open("wb") as output:
                # Bound compressed downloads as well as extracted size.
                total = 0
                while True:
                    chunk = response.read(1024 * 1024)
                    if not chunk:
                        break
                    total += len(chunk)
                    if total > 200 * 1024 * 1024:
                        raise SegModelDownloadError("Segmentation model archive exceeds size limit")
                    output.write(chunk)
            with zipfile.ZipFile(archive) as source:
                total = 0
                for member in source.infolist():
                    path = PurePosixPath(member.filename)
                    if (path.is_absolute() or ".." in path.parts or "\\" in member.filename
                            or not path.parts or path.parts[0] != "seg"
                            or stat.S_ISLNK(member.external_attr >> 16)):
                        raise SegModelDownloadError("Unsafe segmentation model archive path")
                    total += member.file_size
                    if total > 200 * 1024 * 1024:
                        raise SegModelDownloadError("Segmentation model expands beyond size limit")
                for name in MODEL_FILES:
                    output = temporary / "seg" / name
                    output.parent.mkdir(parents=True, exist_ok=True)
                    with source.open("seg/" + name) as handle, output.open("xb") as target:
                        shutil.copyfileobj(handle, target)
            staged = temporary / "seg"
            if not _verified(staged):
                raise SegModelDownloadError("Segmentation model SHA-256 mismatch")
            if force and destination.exists():
                shutil.rmtree(destination)
            try:
                staged.rename(destination)
            except OSError:
                # Another process may have finished the same version concurrently.
                if not _verified(destination):
                    raise
        return destination
    except SegModelDownloadError:
        raise
    except Exception as exc:
        raise SegModelDownloadError("Cannot download the DCCC Seg model; provide bundle= with "
                                    "an extracted DCCCseg Release asset for offline use") from exc


def default_seg_bundle(executable: Path) -> Path | None:
    """Return None for a full native installation, otherwise provision its model."""
    if _complete(Path(executable).resolve().parent / "assets/models/seg"):
        return None
    destination = seg_model_dir()
    if _verified(destination):
        return destination
    if not auto_download_enabled():
        raise SegModelDownloadError("Segmentation model is absent and automatic download is disabled; "
                                    "pass bundle= or call download_seg_model() explicitly")
    return download_seg_model()
