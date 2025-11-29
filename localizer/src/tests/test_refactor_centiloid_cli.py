import pytest


class TestRefactorCentiloidCLI:
    """Tests for the refactor-centiloid prototype command."""

    def test_refactor_centiloid_basic(self, run_subprocess, tmp_path, test_files):
        """
        Ensure the refactored Centiloid pipeline completes successfully.
        """
        output_path = tmp_path / "refactor_centiloid_output.nii"
        args = [
            "refactor-centiloid",
            "--input",
            str(test_files["input"]),
            "--output",
            str(output_path),
        ]

        result = run_subprocess(args)

        assert result.returncode == 0, (
            "refactor-centiloid command failed.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )

    def test_refactor_centiloid_skip_normalization(self, run_subprocess, tmp_path, test_files):
        """
        Ensure the command honors --skip-normalization when a normalized input is provided.
        """
        output_path = tmp_path / "refactor_centiloid_skip.nii"
        args = [
            "refactor-centiloid",
            "--input",
            str(test_files["input"]),
            "--output",
            str(output_path),
            "--skip-normalization",
        ]

        result = run_subprocess(args)

        assert result.returncode == 0, (
            "refactor-centiloid command failed with --skip-normalization.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )

