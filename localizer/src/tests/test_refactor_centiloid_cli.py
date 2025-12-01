import pytest  # type: ignore
import shutil


class TestRefactorCentiloidCLI:
    """Tests for the centiloid prototype command."""

    def test_refactor_centiloid_basic(self, run_subprocess, tmp_path, test_files):
        """
        Ensure the refactored Centiloid pipeline completes successfully.
        """
        output_path = tmp_path / "refactor_centiloid_output.nii"
        args = [
            "centiloid",
            "--input",
            str(test_files["input"]),
            "--output",
            str(output_path),
        ]

        result = run_subprocess(args)

        assert result.returncode == 0, (
            "centiloid command failed.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )

    def test_refactor_centiloid_skip_normalization(self, run_subprocess, tmp_path, test_files):
        """
        Ensure the command honors --skip-normalization when a normalized input is provided.
        """
        output_path = tmp_path / "refactor_centiloid_skip.nii"
        args = [
            "centiloid",
            "--input",
            str(test_files["input"]),
            "--output",
            str(output_path),
            "--skip-normalization",
        ]

        result = run_subprocess(args)

        assert result.returncode == 0, (
            "centiloid command failed with --skip-normalization.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )

    def test_refactor_centiloid_batch(self, run_subprocess, tmp_path, test_files):
        """
        Ensure batch mode processes directory inputs and writes outputs.
        """
        input_dir = tmp_path / "batch_inputs"
        output_dir = tmp_path / "batch_outputs"
        input_dir.mkdir()
        output_dir.mkdir()

        sample_path = input_dir / "sample_centiloid.nii"
        shutil.copy(test_files["input"], sample_path)

        args = [
            "centiloid",
            "--input",
            str(input_dir),
            "--output",
            str(output_dir),
            "--batch",
        ]

        result = run_subprocess(args)

        assert result.returncode == 0, (
            "centiloid batch command failed.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )

        generated = list(output_dir.glob("*_centiloid_refactor.nii"))
        assert generated, "Batch run did not produce any centiloid outputs."
        assert (output_dir / "results.csv").exists(), (
            "Batch run did not emit results.csv"
        )
        assert (output_dir / "batch_info.txt").exists(), (
            "Batch run did not emit batch_info.txt"
        )

