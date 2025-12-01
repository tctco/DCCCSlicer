import pytest  # type: ignore
import shutil


class TestRefactorCentaurzCLI:
    """Tests for the centaurz prototype command."""

    def test_refactor_centaurz_basic(self, run_subprocess, tmp_path, test_files):
        """
        Ensure the refactored CenTauRz pipeline completes successfully.
        """
        output_path = tmp_path / "refactor_centaurz_output.nii"
        args = [
            "centaurz",
            "--input",
            str(test_files["input"]),
            "--output",
            str(output_path),
        ]

        result = run_subprocess(args)

        assert result.returncode == 0, (
            "centaurz command failed.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )

    def test_refactor_centaurz_skip_normalization(self, run_subprocess, tmp_path, test_files):
        """
        Ensure the command honors --skip-normalization when a normalized input is provided.
        """
        output_path = tmp_path / "refactor_centaurz_skip.nii"
        args = [
            "centaurz",
            "--input",
            str(test_files["input"]),
            "--output",
            str(output_path),
            "--skip-normalization",
        ]

        result = run_subprocess(args)

        assert result.returncode == 0, (
            "centaurz command failed with --skip-normalization.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )

    def test_refactor_centaurz_batch(self, run_subprocess, tmp_path, test_files):
        """
        Ensure batch mode processes directories and emits outputs plus logs.
        """
        input_dir = tmp_path / "batch_inputs"
        output_dir = tmp_path / "batch_outputs"
        input_dir.mkdir()
        output_dir.mkdir()

        sample_path = input_dir / "sample_centaurz.nii"
        shutil.copy(test_files["input"], sample_path)

        args = [
            "centaurz",
            "--input",
            str(input_dir),
            "--output",
            str(output_dir),
            "--batch",
        ]

        result = run_subprocess(args)

        assert result.returncode == 0, (
            "centaurz batch command failed.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )

        generated = list(output_dir.glob("*_centaurz_refactor.nii"))
        assert generated, "Batch run did not produce CenTauRz outputs."
        assert (output_dir / "results.csv").exists(), "Batch run missing results.csv"
        assert (output_dir / "batch_info.txt").exists(), "Batch run missing batch_info.txt"

