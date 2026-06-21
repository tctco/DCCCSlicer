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

CENTILOID_ERROR_THRESHOLD = 10.0
CENTAURZ_ERROR_THRESHOLD = 2.0
CENTAURZ_SUVR_RELATIVE_ERROR_THRESHOLD = 0.10

CENTAURZ_REGION_TO_RESULT_METRIC = {
    "universal": "CenTauRz",
    "mesial_temporal": "CenTauRz.MesialTemporal",
    "meta_temporal": "CenTauRz.MetaTemporal",
    "temporo_parietal": "CenTauRz.TemporoParietal",
    "frontal": "CenTauRz.Frontal",
}

# Define TEST_DIR relative to this file to read gt.csv during collection
CURRENT_DIR = Path(__file__).parent.resolve()

def read_gt_data() -> List[Dict[str, Any]]:
    gt_dir = CURRENT_DIR / "test_acc_centiloid_centaurz"
    gt_files = [gt_dir / "gt.csv", gt_dir / "gt_detailed.csv"]
    try:
        frames = [pd.read_csv(path) for path in gt_files if path.exists()]
        if not frames:
            return []
        df = pd.concat(frames, ignore_index=True)
        df = df.drop_duplicates()
        df = df.dropna(subset=['image', 'metric'])
        return df.to_dict('records')
    except Exception as e:
        print(f"Warning: Failed to read Centiloid/CenTauRz gt.csv files using pandas: {e}")
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
        region = self._normalize_region(row.get("region"))
        value_type = self._normalize_value_type(row.get("value_type"))
        
        # Locate source file
        # Inputs are in 'tests/test_acc_centiloid_centaurz'
        src_file = test_files['test_dir'] / "test_acc_centiloid_centaurz" / image_name
        
        if not src_file.exists():
            self._record_result(image_name, metric, tracer, region, value_type, gt_value, "Skip", "N/A")
            pytest.skip(f"Input file {image_name} not found in tests/test_acc_centiloid_centaurz")

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
            self._record_result(image_name, metric, tracer, region, value_type, gt_value, "ERR", "N/A")
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
        if metric.lower() == "centaurz" and region != "universal":
            cmd.append("--report-detailed-regions")
        
        print(f"Running: {' '.join(cmd)}")
        
        # Run subprocess
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode != 0:
            self._record_result(image_name, metric, tracer, region, value_type, gt_value, "FAIL", "N/A")
            pytest.fail(f"Process failed with return code {result.returncode}:\n{result.stderr}")
            
        # Parse output CSV
        results_csv = output_dir / "results.csv"
        if not results_csv.exists():
            self._record_result(image_name, metric, tracer, region, value_type, gt_value, "No CSV", "N/A")
            pytest.fail("results.csv was not created")
            
        pred_value = None
        try:
            res_df = pd.read_csv(results_csv)
            if 'Filename' in res_df.columns:
                file_row = res_df[res_df['Filename'] == image_name]
                result_metric_name = self._resolve_result_metric_name(metric, region)
                if result_metric_name and 'Metric' in res_df.columns:
                    file_row = file_row[file_row['Metric'] == result_metric_name]
                if not file_row.empty:
                    result_column = self._resolve_result_column(metric, tracer, value_type)
                    if result_column in file_row.columns:
                         val = file_row.iloc[0][result_column]
                         if pd.notna(val):
                             pred_value = float(val)
        except Exception as e:
            self._record_result(image_name, metric, tracer, region, value_type, gt_value, "ReadErr", "N/A")
            pytest.fail(f"Error reading results.csv: {e}")
            
        if pred_value is None:
            self._record_result(image_name, metric, tracer, region, value_type, gt_value, "No Val", "N/A")
            pytest.fail(
                f"Value for {metric}/{region}/{value_type} ({tracer}) "
                f"not found in results.csv or is invalid"
            )
            
        diff = pred_value - gt_value
        self._record_result(image_name, metric, tracer, region, value_type, gt_value, pred_value, diff)
        
        abs_diff = abs(diff)
        if metric.lower() == 'centiloid':
            assert abs_diff <= CENTILOID_ERROR_THRESHOLD, f"Centiloid error {abs_diff:.2f} > {CENTILOID_ERROR_THRESHOLD}"
        elif metric.lower() == 'centaurz':
            if value_type == "suvr":
                relative_error = abs_diff / abs(gt_value) if abs(gt_value) > 1e-8 else abs_diff
                assert relative_error <= CENTAURZ_SUVR_RELATIVE_ERROR_THRESHOLD, (
                    f"CenTauRz SUVr relative error {relative_error:.4f} > "
                    f"{CENTAURZ_SUVR_RELATIVE_ERROR_THRESHOLD}"
                )
            else:
                assert abs_diff <= CENTAURZ_ERROR_THRESHOLD, (
                    f"Centaurz error {abs_diff:.2f} > {CENTAURZ_ERROR_THRESHOLD}"
                )

    @staticmethod
    def _normalize_region(region_value: Any) -> str:
        if pd.isna(region_value):
            return "universal"
        normalized = str(region_value).strip().lower().replace(" ", "_").replace("-", "_")
        return normalized or "universal"

    @staticmethod
    def _normalize_value_type(value_type: Any) -> str:
        if pd.isna(value_type):
            return "metric"
        normalized = str(value_type).strip().lower()
        return normalized or "metric"

    @staticmethod
    def _resolve_result_metric_name(metric: str, region: str) -> str:
        if metric.lower() != "centaurz":
            return ""
        if region not in CENTAURZ_REGION_TO_RESULT_METRIC:
            raise AssertionError(f"Unsupported CenTauRz region in gt.csv: {region}")
        return CENTAURZ_REGION_TO_RESULT_METRIC[region]

    @staticmethod
    def _resolve_result_column(metric: str, tracer: str, value_type: str) -> str:
        if metric.lower() == "centaurz" and value_type == "suvr":
            return "SUVr"
        return tracer

    def _record_result(self, image, metric, tracer, region, value_type, gt, pred, diff):
        """Helper to format and record results."""
        def fmt(val):
            if isinstance(val, (int, float)):
                return f"{val:.1f}"
            return str(val)

        metric_label = metric
        if metric.lower() == "centaurz":
            metric_label = f"{metric}:{region}:{value_type}"

        ACC_TEST_RESULTS.append({
            'image': image,
            'metric': metric_label,
            'tracer': tracer,
            'gt': fmt(gt),
            'pred': fmt(pred),
            'diff': fmt(diff)
        })
