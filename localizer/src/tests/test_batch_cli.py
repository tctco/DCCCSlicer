import pytest
import subprocess
import sys
from pathlib import Path

class TestBatchProcessing:
    """
    Test class for batch processing functionality of CentiloidCalculator.
    """

    def test_batch_processing_success(self, exe_path, data_dir, output_dir, run_subprocess):
        """
        Test that the batch processing command runs successfully and produces output.
        Equivalent to the original test_batch() function.
        """
        # Ensure input directory is valid (or populate it if needed)
        assert data_dir.exists(), "Input directory must exist"
        
        # Construct arguments
        # Original: ["centiloid", "--input", "./inputs", "--output", "./outputs", "--batch"]
        # We use absolute paths from fixtures to be safe
        args = [
            "centiloid",
            "--input", str(data_dir),
            "--output", str(output_dir),
            "--tracer", "fbp",
            "--batch"
        ]
        
        # Run command
        result = run_subprocess(args)
        
        # Assertions
        assert result.returncode == 0, f"Command failed with stderr: {result.stderr}"
        
        # Verify outputs were created (if input dir was not empty)
        # If input dir is empty, the tool might still exit 0 but produce nothing.
        # Depending on the tool's behavior, we might want to check stdout.
        if any(data_dir.iterdir()):
            # Check for required batch output files
            results_csv = output_dir / "results.csv"
            batch_info = output_dir / "batch_info.txt"
            
            assert results_csv.exists(), "results.csv was not created in output directory"
            assert batch_info.exists(), "batch_info.txt was not created in output directory"

    def test_batch_processing_missing_input(self, exe_path, output_dir, run_subprocess, tmp_path):
        """
        Test behavior when input directory does not exist or is invalid.
        """
        non_existent_input = tmp_path / "non_existent_inputs"
        
        args = [
            "centiloid",
            "--input", str(non_existent_input),
            "--output", str(output_dir),
            "--tracer", "fbp",
            "--batch"
        ]
        
        result = run_subprocess(args)
        
        # Expect failure or specific error message
        assert result.returncode != 0 or "Error" in result.stderr or "Error" in result.stdout
