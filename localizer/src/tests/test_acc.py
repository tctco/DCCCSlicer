import pytest
import subprocess
import shutil
import pandas as pd
from pathlib import Path
from typing import List, Dict, Any
# Import the shared results list from conftest (via imports or directly if exposed)
# Since fixtures can't easily "return" to global scope across modules without a shared mutable object,
# we rely on the one defined in conftest_plugins which conftest.py imports.
from conftest_plugins import ACC_TEST_RESULTS

# Define TEST_DIR relative to this file to read gt.csv during collection
CURRENT_DIR = Path(__file__).parent.resolve()

def read_gt_data() -> List[Dict[str, Any]]:
    gt_file = CURRENT_DIR / "gt.csv"
    if not gt_file.exists():
        return []
    try:
        # Read CSV using pandas
        df = pd.read_csv(gt_file)
        # Filter out rows with missing image or metric
        df = df.dropna(subset=['image', 'metric'])
        # Convert to list of dicts
        return df.to_dict('records')
    except Exception as e:
        print(f"Warning: Failed to read gt.csv using pandas: {e}")
        return []

GT_DATA = read_gt_data()

class TestEvaluation:
    
    @pytest.mark.parametrize("row", GT_DATA)
    def test_quantification_accuracy(self, row, exe_path, test_files, tmp_path):
        """
        Run quantification for a single image and compare with ground truth.
        """
        image_name = row['image']
        metric = row['metric']
        tracer = row['tracer']
        gt_value = float(row['value'])
        
        # Locate source file
        # Inputs are in 'tests/test_acc'
        src_file = test_files['test_dir'] / "test_acc" / image_name
        
        if not src_file.exists():
            self._record_result(image_name, metric, tracer, gt_value, "Skip", "N/A")
            pytest.skip(f"Input file {image_name} not found in tests/test_acc")

        # Prepare isolated environment for batch mode to ensure clean CSV and no interference
        input_dir = tmp_path / "inputs"
        input_dir.mkdir()
        output_dir = tmp_path / "outputs"
        output_dir.mkdir()
        
        # Copy file to input dir
        shutil.copy(src_file, input_dir / image_name)
        
        # Determine subcommand
        if metric.lower() == 'centiloid':
            subcmd = 'centiloid'
        elif metric.lower() == 'centaurz':
            subcmd = 'centaurz'
        else:
            self._record_result(image_name, metric, tracer, gt_value, "ERR", "N/A")
            pytest.fail(f"Unknown metric: {metric}")

        # Construct command
        # Using batch mode to generate results.csv
        cmd = [
            exe_path,
            subcmd,
            "--input", str(input_dir),
            "--output", str(output_dir),
            "--batch"
        ]
        
        print(f"Running: {' '.join(cmd)}")
        
        # Run subprocess
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode != 0:
            self._record_result(image_name, metric, tracer, gt_value, "FAIL", "N/A")
            pytest.fail(f"Process failed with return code {result.returncode}:\n{result.stderr}")
            
        # Parse output CSV
        results_csv = output_dir / "results.csv"
        if not results_csv.exists():
            self._record_result(image_name, metric, tracer, gt_value, "No CSV", "N/A")
            pytest.fail("results.csv was not created")
            
        pred_value = None
        try:
            # Use pandas to read results.csv as well for consistency
            res_df = pd.read_csv(results_csv)
            # Find the row for our image
            # Filename column might be just filename or full path depending on implementation
            # Implementation says: csvFile << inputFile.filename().string()
            
            # Check if 'Filename' column exists
            if 'Filename' in res_df.columns:
                file_row = res_df[res_df['Filename'] == image_name]
                if not file_row.empty:
                    if tracer in file_row.columns:
                         val = file_row.iloc[0][tracer]
                         if pd.notna(val):
                             pred_value = float(val)
        except Exception as e:
            self._record_result(image_name, metric, tracer, gt_value, "ReadErr", "N/A")
            pytest.fail(f"Error reading results.csv: {e}")
            
        if pred_value is None:
            self._record_result(image_name, metric, tracer, gt_value, "No Val", "N/A")
            pytest.fail(f"Value for tracer {tracer} not found in results.csv or is invalid")
            
        diff = pred_value - gt_value
        self._record_result(image_name, metric, tracer, gt_value, pred_value, diff)
        
        # Accuracy Assertion
        abs_diff = abs(diff)
        if metric.lower() == 'centiloid':
            assert abs_diff <= 10.0, f"Centiloid error {abs_diff:.2f} > 10.0"
        elif metric.lower() == 'centaurz':
            assert abs_diff <= 2.0, f"Centaurz error {abs_diff:.2f} > 2.0"

    def _record_result(self, image, metric, tracer, gt, pred, diff):
        """Helper to format and record results."""
        def fmt(val):
            if isinstance(val, (int, float)):
                return f"{val:.1f}"
            return str(val)
            
        ACC_TEST_RESULTS.append({
            'image': image,
            'metric': metric,
            'tracer': tracer,
            'gt': fmt(gt),
            'pred': fmt(pred),
            'diff': fmt(diff)
        })
