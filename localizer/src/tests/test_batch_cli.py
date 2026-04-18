import pytest
import subprocess
import sys
from pathlib import Path

class TestBatchProcessing:
    """
    Test class for batch processing functionality of DCCCcore.
    """

    @pytest.mark.parametrize(
        ("subcommand", "expects_results_csv"),
        [("centiloid", True), ("adni-pet-core", False)],
    )
    def test_batch_processing_success(
        self,
        subcommand,
        expects_results_csv,
        exe_path,
        data_dir,
        output_dir,
        run_subprocess,
    ):
        """
        Test that the batch processing command runs successfully and produces output.
        Equivalent to the original test_batch() function.
        """
        # Ensure input directory is valid (or populate it if needed)
        assert data_dir.exists(), "Input directory must exist"
        
        args = [
            subcommand,
            "--input", str(data_dir),
            "--output", str(output_dir),
            "--batch"
        ]
        
        result = run_subprocess(args)
        
        assert result.returncode == 0, (
            f"{subcommand} batch command failed.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )
        
        if any(data_dir.iterdir()):
            batch_info = output_dir / "batch_info.txt"
            assert batch_info.exists(), "batch_info.txt was not created in output directory"

            if expects_results_csv:
                results_csv = output_dir / "results.csv"
                assert results_csv.exists(), "results.csv was not created in output directory"

            generated = list(output_dir.glob("*.nii"))
            assert generated, f"{subcommand} batch run did not produce any output .nii files."

    @pytest.mark.parametrize("subcommand", ["centiloid", "adni-pet-core"])
    def test_batch_processing_missing_input(self, subcommand, exe_path, output_dir, run_subprocess, tmp_path):
        """
        Test behavior when input directory does not exist or is invalid.
        """
        non_existent_input = tmp_path / "non_existent_inputs"
        
        args = [
            subcommand,
            "--input", str(non_existent_input),
            "--output", str(output_dir),
            "--batch"
        ]
        
        result = run_subprocess(args)
        
        # Expect failure or specific error message
        assert result.returncode != 0 or "Error" in result.stderr or "Error" in result.stdout
