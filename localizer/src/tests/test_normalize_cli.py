import pytest

class TestNormalizeCLI:
    """Standard normalize command coverage."""

    def test_basic(self, run_subprocess, tmp_path, test_files):
        output_path = tmp_path / "normalize_output.nii"
        result = run_subprocess(
            [
                "normalize",
                "--input",
                str(test_files["input"]),
                "--output",
                str(output_path),
            ]
        )

        assert result.returncode == 0, (
            "normalize command failed.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )
        assert output_path.exists(), "normalize command did not create the expected output file."

    def test_requires_input(self, run_subprocess, tmp_path):
        output_path = tmp_path / "normalize_missing_input.nii"
        result = run_subprocess(
            [
                "normalize",
                "--output",
                str(output_path),
            ]
        )

        assert result.returncode != 0, (
            "normalize command should fail when --input is omitted.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )

    def test_iterative_flag(self, run_subprocess, tmp_path, test_files):
        output_path = tmp_path / "normalize_iterative_output.nii"
        result = run_subprocess(
            [
                "normalize",
                "--input",
                str(test_files["input"]),
                "--output",
                str(output_path),
                "--iterative",
            ]
        )

        assert result.returncode == 0, (
            "normalize command with --iterative failed.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )
        assert output_path.exists(), "normalize --iterative did not create the expected output file."

