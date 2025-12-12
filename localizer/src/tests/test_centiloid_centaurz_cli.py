import shutil
import pytest
class TestCentiloidAndCentaurzCLI:
    """Basic CLI coverage for centiloid and centaurz commands."""

    @pytest.mark.parametrize("subcommand", ["centiloid", "centaurz"])
    def test_basic_invocation(self, subcommand, run_subprocess, tmp_path, test_files):
        output_path = tmp_path / f"{subcommand}_basic_output.nii"
        result = run_subprocess(
            [
                subcommand,
                "--input",
                str(test_files["input"]),
                "--output",
                str(output_path),
            ]
        )

        assert result.returncode == 0, (
            f"{subcommand} command failed.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )

    @pytest.mark.parametrize("subcommand", ["centiloid", "centaurz"])
    def test_skip_normalization(self, subcommand, run_subprocess, tmp_path, test_files):
        output_path = tmp_path / f"{subcommand}_skip_output.nii"
        result = run_subprocess(
            [
                subcommand,
                "--input",
                str(test_files["input"]),
                "--output",
                str(output_path),
                "--skip-normalization",
            ]
        )

        assert result.returncode == 0, (
            f"{subcommand} command failed with --skip-normalization.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )

    @pytest.mark.parametrize(
        ("subcommand", "glob_suffix"),
        [("centiloid", "*.nii"), ("centaurz", "*.nii")],
    )
    def test_batch_mode_outputs(self, subcommand, glob_suffix, run_subprocess, tmp_path, test_files):
        input_dir = tmp_path / f"{subcommand}_batch_inputs"
        output_dir = tmp_path / f"{subcommand}_batch_outputs"
        input_dir.mkdir()
        output_dir.mkdir()

        shutil.copy(test_files["input"], input_dir / f"sample_{subcommand}.nii")

        result = run_subprocess(
            [
                subcommand,
                "--input",
                str(input_dir),
                "--output",
                str(output_dir),
                "--batch",
            ]
        )

        assert result.returncode == 0, (
            f"{subcommand} batch command failed.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )

        generated = list(output_dir.glob(glob_suffix))
        assert generated, f"Batch run did not produce any {subcommand} outputs."
        assert (output_dir / "results.csv").exists(), "Batch run missing results.csv"
        assert (output_dir / "batch_info.txt").exists(), "Batch run missing batch_info.txt"