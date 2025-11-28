import pytest
import subprocess
import pandas as pd
from pathlib import Path
from typing import List, Dict, Any

from conftest_plugins import ACC_TEST_RESULTS


ERROR_THRESHOLD = 0.03


CURRENT_DIR = Path(__file__).parent.resolve()


def read_gt_data() -> List[Dict[str, Any]]:
    """
    Read ground-truth fill states data from CSV.
    """
    gt_file = CURRENT_DIR / "test_acc_fill_states" / "gt.csv"
    if not gt_file.exists():
        return []
    try:
        df = pd.read_csv(gt_file)
        # Drop rows with missing filename or fill state
        df = df.dropna(subset=["fname", "fill state", "tracer"])
        return df.to_dict("records")
    except Exception as e:
        print(f"Warning: Failed to read gt.csv using pandas: {e}")
        return []


GT_DATA = read_gt_data()


class TestFillStatesAccuracy:
    @pytest.mark.parametrize("row", GT_DATA)
    def test_fillstates_accuracy(self, row, exe_path, test_files, tmp_path):
        """
        Run fillstates for each image and compare with ground-truth fill state.
        """
        image_name = row["fname"]
        tracer = row["tracer"]
        gt_value = float(row["fill state"])

        # Locate source file under tests/test_acc_fill_states
        src_file = test_files["test_dir"] / "test_acc_fill_states" / image_name

        if not src_file.exists():
            self._record_result(image_name, tracer, gt_value, "Skip", "N/A")
            pytest.skip(
                f"Input file {image_name} not found in tests/test_acc_fill_states"
            )

        # Prepare output path in isolated tmp directory
        output_path = tmp_path / f"output_fillstates_{Path(image_name).stem}.nii"

        # Tracer argument is expected in lowercase (e.g., fdg, fbp, ftp)
        tracer_arg = str(tracer).lower()

        cmd = [
            exe_path,
            "fillstates",
            "--input",
            str(src_file),
            "--output",
            str(output_path),
            "--tracer",
            tracer_arg,
        ]

        print(f"Running: {' '.join(cmd)}")

        result = subprocess.run(cmd, capture_output=True, text=True)

        if result.returncode != 0:
            self._record_result(image_name, tracer, gt_value, "FAIL", "N/A")
            pytest.fail(
                f"fillstates failed with return code {result.returncode}:\n"
                f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
            )

        # Parse stdout to extract the predicted fill state for the tracer
        pred_value = self._parse_fillstate_from_stdout(result.stdout, tracer)

        if pred_value is None:
            self._record_result(image_name, tracer, gt_value, "No Val", "N/A")
            pytest.fail(
                f"Value for tracer {tracer} not found in fillstates output "
                f"or could not be parsed.\nSTDOUT:\n{result.stdout}"
            )

        diff = pred_value - gt_value
        self._record_result(image_name, tracer, gt_value, pred_value, diff)

        abs_diff = abs(diff)
        assert (
            abs_diff <= ERROR_THRESHOLD
        ), f"FillStates error {abs_diff:.4f} > {ERROR_THRESHOLD}"

    def _parse_fillstate_from_stdout(self, stdout: str, tracer: str):
        """
        Parse the fill state value for a given tracer from stdout.
        Expected format (example):

        === fillstates Results ===
        Metric: FillStates
        FDG: 0.000945358
        """
        tracer_upper = str(tracer).upper()
        for line in stdout.splitlines():
            stripped = line.strip()
            if not stripped:
                continue
            if stripped.startswith(f"{tracer_upper}:"):
                try:
                    _, value_str = stripped.split(":", 1)
                    return float(value_str.strip())
                except ValueError:
                    continue
        return None

    def _record_result(self, image, tracer, gt, pred, diff):
        """Helper to format and record results for the terminal summary table."""

        def fmt(val):
            if isinstance(val, (int, float)):
                return f"{val:.4f}"
            return str(val)

        ACC_TEST_RESULTS.append(
            {
                "image": image,
                "metric": "FillStates",
                "tracer": tracer,
                "gt": fmt(gt),
                "pred": fmt(pred),
                "diff": fmt(diff),
            }
        )


