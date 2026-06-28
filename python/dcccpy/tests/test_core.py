from __future__ import annotations

import os
from pathlib import Path

import pytest

import dcccpy


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
