import pytest


class TestRefactorNormalizeCLI:
    """Tests for the refactored standard normalize command."""

    def test_normalize_basic(self, run_subprocess, tmp_path, test_files):
        """
        Ensure the vanilla normalize flow runs end-to-end with default flags.
        """
        output_path = tmp_path / "normalize_output.nii"
        args = [
            "normalize",
            "--input",
            str(test_files["input"]),
            "--output",
            str(output_path),
        ]

        result = run_subprocess(args)

        assert result.returncode == 0, (
            "normalize command failed.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )
        assert output_path.exists(), "normalize command did not create the expected output file."

    def test_normalize_requires_input(self, run_subprocess, tmp_path):
        """
        Normalize should fail when required positional arguments are missing.
        """
        output_path = tmp_path / "normalize_missing_input.nii"
        args = [
            "normalize",
            "--output",
            str(output_path),
        ]

        result = run_subprocess(args)

        assert result.returncode != 0, (
            "normalize command should fail when --input is omitted.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )

    def test_normalize_iterative_flag(self, run_subprocess, tmp_path, test_files):
        """
        Verify the iterative registration flag is accepted by the CLI.
        """
        output_path = tmp_path / "normalize_iterative_output.nii"
        args = [
            "normalize",
            "--input",
            str(test_files["input"]),
            "--output",
            str(output_path),
            "--iterative",
        ]

        result = run_subprocess(args)

        assert result.returncode == 0, (
            "normalize command with --iterative failed.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )
        assert output_path.exists(), "normalize --iterative did not create the expected output file."

