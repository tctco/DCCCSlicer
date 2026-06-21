import shutil
import subprocess
import sys
from pathlib import Path

import pytest


pytestmark = pytest.mark.skipif(
    sys.platform != "win32",
    reason="Windows-specific Unicode path regression tests",
)


def _resolve_runtime_config(exe_path: str) -> Path:
    exe = Path(exe_path).resolve()
    runtime_config = exe.parent / "assets" / "configs" / "config.toml"
    if runtime_config.exists():
        return runtime_config

    source_config = Path(__file__).resolve().parent.parent / "assets" / "configs" / "config.toml"
    if source_config.exists():
        return source_config

    raise FileNotFoundError("Unable to locate config.toml for Unicode path tests.")


def _decode_output(data: bytes | None) -> str:
    if not data:
        return ""

    for encoding in ("utf-8", "gb18030", sys.getfilesystemencoding()):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            continue

    return data.decode("utf-8", errors="replace")


def _run_cli(args: list[str], cwd: Path | None = None) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        args,
        capture_output=True,
        text=False,
        check=False,
        cwd=cwd,
    )


class TestUnicodePathsCLI:
    def test_normalize_with_unicode_config_input_and_output_paths(
        self,
        exe_path,
        tmp_path,
        test_files,
    ):
        case_dir = tmp_path / "中文路径用例"
        input_dir = case_dir / "输入目录"
        output_dir = case_dir / "输出目录"
        config_dir = case_dir / "配置目录"
        input_dir.mkdir(parents=True)
        output_dir.mkdir(parents=True)
        config_dir.mkdir(parents=True)

        input_path = input_dir / "示例输入.nii"
        output_path = output_dir / "示例输出.nii"
        config_path = config_dir / "配置.toml"

        shutil.copy(test_files["input"], input_path)
        shutil.copy(_resolve_runtime_config(exe_path), config_path)

        result = _run_cli(
            [
                exe_path,
                "normalize",
                "--input",
                str(input_path),
                "--output",
                str(output_path),
                "--config",
                str(config_path),
            ]
        )
        stdout = _decode_output(result.stdout)
        stderr = _decode_output(result.stderr)

        assert result.returncode == 0, (
            "normalize command failed for Unicode config/input/output paths.\n"
            f"STDOUT:\n{stdout}\nSTDERR:\n{stderr}"
        )
        assert output_path.exists(), "Output file was not created under a Unicode path."

    def test_normalize_with_unicode_runtime_directory(
        self,
        exe_path,
        tmp_path,
        test_files,
    ):
        source_bin_dir = Path(exe_path).resolve().parent
        source_runtime_config = source_bin_dir / "assets" / "configs" / "config.toml"
        if not source_runtime_config.exists():
            pytest.skip("Installed runtime assets are required for the Unicode runtime directory test.")

        runtime_root = tmp_path / "中文运行目录"
        runtime_bin_dir = runtime_root / source_bin_dir.name
        shutil.copytree(source_bin_dir, runtime_bin_dir)

        runtime_exe = runtime_bin_dir / Path(exe_path).name
        input_dir = runtime_root / "输入数据"
        output_dir = runtime_root / "输出数据"
        input_dir.mkdir(parents=True)
        output_dir.mkdir(parents=True)

        input_path = input_dir / "运行目录输入.nii"
        output_path = output_dir / "运行目录输出.nii"
        shutil.copy(test_files["input"], input_path)

        result = _run_cli(
            [
                str(runtime_exe),
                "normalize",
                "--input",
                str(input_path),
                "--output",
                str(output_path),
            ],
            cwd=runtime_bin_dir,
        )
        stdout = _decode_output(result.stdout)
        stderr = _decode_output(result.stderr)

        assert result.returncode == 0, (
            "normalize command failed when the runtime directory contained Unicode characters.\n"
            f"STDOUT:\n{stdout}\nSTDERR:\n{stderr}"
        )
        assert output_path.exists(), "Output file was not created from a Unicode runtime directory."
