import pytest


class TestRefactorCentaurzCLI:
    """Tests for the refactor-centaurz prototype command."""

    def test_refactor_centaurz_basic(self, run_subprocess, tmp_path, test_files):
        """
        Ensure the refactored CenTauRz pipeline completes successfully.
        """
        output_path = tmp_path / "refactor_centaurz_output.nii"
        args = [
            "refactor-centaurz",
            "--input",
            str(test_files["input"]),
            "--output",
            str(output_path),
        ]

        result = run_subprocess(args)

        assert result.returncode == 0, (
            "refactor-centaurz command failed.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )

    def test_refactor_centaurz_skip_normalization(self, run_subprocess, tmp_path, test_files):
        """
        Ensure the command honors --skip-normalization when a normalized input is provided.
        """
        output_path = tmp_path / "refactor_centaurz_skip.nii"
        args = [
            "refactor-centaurz",
            "--input",
            str(test_files["input"]),
            "--output",
            str(output_path),
            "--skip-normalization",
        ]

        result = run_subprocess(args)

        assert result.returncode == 0, (
            "refactor-centaurz command failed with --skip-normalization.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )


