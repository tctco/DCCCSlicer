from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
import time
import venv
from pathlib import Path

import pytest


RUN_ENV = "DCCCPY_RUN_TESTPYPI_CENTILOID"
VERSION_ENV = "DCCCPY_TESTPYPI_VERSION"
TESTPYPI_INDEX = "https://test.pypi.org/simple"
PYPI_INDEX = "https://pypi.org/simple"


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def _project_version() -> str:
    pyproject = _repo_root() / "python" / "dcccpy" / "pyproject.toml"
    in_project = False
    for line in pyproject.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if stripped == "[project]":
            in_project = True
            continue
        if in_project and stripped.startswith("["):
            break
        if in_project:
            match = re.match(r'version\s*=\s*"([^"]+)"', stripped)
            if match:
                return match.group(1)
    raise AssertionError(f"Could not read project version from {pyproject}")


def _venv_python(venv_dir: Path) -> Path:
    if sys.platform.startswith("win"):
        return venv_dir / "Scripts" / "python.exe"
    return venv_dir / "bin" / "python"


def _run(command: list[str], *, env: dict[str, str] | None = None, timeout: int = 300) -> subprocess.CompletedProcess[str]:
    completed = subprocess.run(
        command,
        env=env,
        text=True,
        capture_output=True,
        timeout=timeout,
        check=False,
    )
    assert completed.returncode == 0, (
        f"Command failed: {' '.join(command)}\n"
        f"STDOUT:\n{completed.stdout}\n"
        f"STDERR:\n{completed.stderr}"
    )
    return completed


def _run_with_retries(
    command: list[str],
    *,
    attempts: int = 3,
    delay_seconds: int = 20,
    timeout: int = 300,
) -> subprocess.CompletedProcess[str]:
    last: subprocess.CompletedProcess[str] | None = None
    for attempt in range(attempts):
        completed = subprocess.run(
            command,
            text=True,
            capture_output=True,
            timeout=timeout,
            check=False,
        )
        if completed.returncode == 0:
            return completed
        last = completed
        if attempt + 1 < attempts:
            time.sleep(delay_seconds)

    assert last is not None
    assert last.returncode == 0, (
        f"Command failed after {attempts} attempts: {' '.join(command)}\n"
        f"STDOUT:\n{last.stdout}\n"
        f"STDERR:\n{last.stderr}"
    )
    return last


def _create_seeded_venv(venv_dir: Path) -> None:
    uv = shutil.which("uv")
    if uv:
        _run([uv, "venv", "--seed", os.fspath(venv_dir)], timeout=120)
        return

    venv.EnvBuilder(with_pip=True).create(venv_dir)


@pytest.mark.skipif(
    os.environ.get(RUN_ENV) != "1",
    reason=f"set {RUN_ENV}=1 to install dcccpy from TestPyPI and run DCCCcore centiloid",
)
def test_testpypi_install_can_run_centiloid(tmp_path: Path) -> None:
    input_path = _repo_root() / "localizer" / "src" / "tests" / "test_acc_centiloid_centaurz" / "AD01_PiB_5070.nii"
    if not input_path.exists():
        pytest.skip(f"Centiloid test input is missing: {input_path}")

    version = os.environ.get(VERSION_ENV, _project_version())
    venv_dir = tmp_path / "venv"
    work_dir = tmp_path / "work"
    output_path = work_dir / "out" / "centiloid-output.nii"
    cache_dir = tmp_path / "dcccpy-cache"
    work_dir.mkdir()
    shutil.copy(input_path, work_dir / "input.nii")

    _create_seeded_venv(venv_dir)
    python = _venv_python(venv_dir)

    _run_with_retries(
        [
            os.fspath(python),
            "-m",
            "pip",
            "install",
            "--no-cache-dir",
            "--index-url",
            TESTPYPI_INDEX,
            "--extra-index-url",
            PYPI_INDEX,
            f"dcccpy=={version}",
        ],
        attempts=6,
        delay_seconds=20,
        timeout=300,
    )

    env = {
        **os.environ,
        "DCCCPY_AUTO_DOWNLOAD": "1",
        "DCCCPY_CACHE_DIR": os.fspath(cache_dir),
        "DCCCPY_TEST_WORK_DIR": os.fspath(work_dir),
    }
    script = """
from importlib.metadata import metadata, version
from pathlib import Path
import os

import dcccpy
import nibabel

requires = metadata("dcccpy").get_all("Requires-Dist") or []
assert any(req.startswith("nibabel") for req in requires), requires

work_dir = Path(os.environ["DCCCPY_TEST_WORK_DIR"])
os.chdir(work_dir)

result = dcccpy.centiloid("./input.nii", "./out/centiloid-output.nii")
assert result.returncode == 0, result.stderr
assert Path(result.args[2]).is_absolute(), result.args
assert Path(result.args[4]).is_absolute(), result.args

output_path = work_dir / "out" / "centiloid-output.nii"
assert output_path.exists(), output_path
assert output_path.stat().st_size > 0, output_path

image = result.load_output()

print("dcccpy", version("dcccpy"))
print("nibabel", nibabel.__version__)
print("dccccore", result.executable)
print("output", output_path)
print("output_shape", image.shape)
print("metrics", dict(result.metrics))
print(result.stdout)
"""
    completed = _run([os.fspath(python), "-c", script], env=env, timeout=900)
    assert "dcccpy " in completed.stdout
    assert "nibabel " in completed.stdout
    assert "output_shape" in completed.stdout
