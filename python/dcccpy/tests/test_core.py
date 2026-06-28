from __future__ import annotations

import os
from pathlib import Path
import zipfile

import pytest

import dcccpy
from dcccpy.runtime import DCCCcoreNotFoundError, dccccore_path


def make_fake_dccccore(tmp_path: Path) -> Path:
    exe = tmp_path / "DCCCcore"
    exe.write_text(
        "#!/usr/bin/env sh\n"
        "printf '%s\\n' \"$@\" > \"$DCCCPY_FAKE_ARGS\"\n"
        "out=''\n"
        "while [ \"$#\" -gt 0 ]; do\n"
        "  if [ \"$1\" = '--output' ]; then shift; out=\"$1\"; fi\n"
        "  shift || true\n"
        "done\n"
        "if [ -n \"$out\" ]; then mkdir -p \"$(dirname \"$out\")\"; printf 'fake' > \"$out\"; fi\n"
        "printf 'Metric: Centiloid\\n  fbp: 12.5\\n  SUVr: 1.23\\n'\n"
    )
    exe.chmod(0o755)
    return exe


def test_parse_metrics() -> None:
    metrics = dcccpy.parse_metrics(
        "Metric: Centiloid\n"
        "  fbp: 12.5\n"
        "  SUVr: 1.23\n"
        "ADAD score: -0.2\n"
    )
    assert metrics == {"fbp": 12.5, "SUVr": 1.23, "ADAD score": -0.2}


def test_centiloid_omits_output_by_creating_temp_path(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    fake_args = tmp_path / "args.txt"
    fake_exe = make_fake_dccccore(tmp_path)
    monkeypatch.setenv("DCCCPY_DCCCCORE", str(fake_exe))
    monkeypatch.setenv("DCCCPY_FAKE_ARGS", str(fake_args))

    result = dcccpy.centiloid("input.nii", skip_normalization=True)

    assert result.returncode == 0
    assert result.output is not None
    assert result.output.exists()
    assert result.temp_dir is not None
    assert result.output.parent == result.temp_dir
    assert result.metrics["fbp"] == 12.5

    args = fake_args.read_text().splitlines()
    assert args[:5] == ["centiloid", "--input", "input.nii", "--output", str(result.output)]
    assert "--skip-normalization" in args


def test_nibabel_like_input_is_saved_to_temp_file(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    class FakeImage:
        def __init__(self) -> None:
            self.saved_to: Path | None = None

        def to_filename(self, filename: str) -> None:
            self.saved_to = Path(filename)
            self.saved_to.write_text("fake nifti")

    fake_args = tmp_path / "args.txt"
    fake_exe = make_fake_dccccore(tmp_path)
    image = FakeImage()
    output = tmp_path / "output.nii"
    monkeypatch.setenv("DCCCPY_DCCCCORE", str(fake_exe))
    monkeypatch.setenv("DCCCPY_FAKE_ARGS", str(fake_args))

    result = dcccpy.centiloid(image, output)

    assert result.returncode == 0
    assert result.output == output
    assert result.temp_dir is not None
    assert image.saved_to is not None
    assert image.saved_to.exists()

    args = fake_args.read_text().splitlines()
    assert args[:5] == ["centiloid", "--input", str(image.saved_to), "--output", str(output)]


def test_dccccore_path_can_disable_auto_download(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("DCCCPY_DCCCCORE", raising=False)
    monkeypatch.setenv("DCCCPY_AUTO_DOWNLOAD", "0")
    monkeypatch.setenv("DCCCPY_CACHE_DIR", str(tmp_path / "cache"))
    monkeypatch.setenv("PATH", str(tmp_path))

    with pytest.raises(DCCCcoreNotFoundError):
        dccccore_path()


def test_dccccore_path_auto_downloads_to_cache(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    asset_root = tmp_path / "asset" / "DCCCcore-4.2.3-ubuntu-latest-x64"
    asset_root.mkdir(parents=True)
    exe = asset_root / "DCCCcore"
    exe.write_text("#!/usr/bin/env sh\nprintf 'fake dccccore\\n'\n")
    exe.chmod(0o755)

    archive = tmp_path / "DCCCcore.zip"
    with zipfile.ZipFile(archive, "w") as zf:
        zf.write(exe, "DCCCcore-4.2.3-ubuntu-latest-x64/DCCCcore")

    monkeypatch.delenv("DCCCPY_DCCCCORE", raising=False)
    monkeypatch.setenv("DCCCPY_AUTO_DOWNLOAD", "1")
    monkeypatch.setenv("DCCCPY_CACHE_DIR", str(tmp_path / "cache"))
    monkeypatch.setenv("DCCCPY_DCCCCORE_URL", archive.as_uri())
    monkeypatch.setenv("PATH", str(tmp_path))

    resolved = dccccore_path()

    assert resolved.exists()
    assert resolved.read_text().startswith("#!/usr/bin/env sh")
    assert str(tmp_path / "cache") in str(resolved)


def test_cli_forwards_raw_args(tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]) -> None:
    fake_args = tmp_path / "args.txt"
    fake_exe = make_fake_dccccore(tmp_path)
    monkeypatch.setenv("DCCCPY_DCCCCORE", str(fake_exe))
    monkeypatch.setenv("DCCCPY_FAKE_ARGS", str(fake_args))

    from dcccpy.cli import main

    code = main(["--help"])

    assert code == 0
    assert fake_args.read_text().splitlines() == ["--help"]
    assert "Metric: Centiloid" in capsys.readouterr().out
