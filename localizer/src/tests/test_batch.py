from pathlib import Path
import shutil
from quick_test import run_command

TEST_DIR = Path(__file__).parent

def test_batch():
    exe_path = '../install/bin/CentiloidCalculator'
    input_dir = TEST_DIR / "inputs"
    assert input_dir.exists(), "Input directory does not exist"
    output_dir = TEST_DIR / "outputs"
    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    test_case = {
        "name": "Batch Processing",
        "args": ["centiloid", "--input", "./inputs", "--output", "./outputs", "--batch"],
        "description": "Batch processing of multiple input files",
        "expected_failure": False
    }
    success = run_command(exe_path, test_case["args"], test_case["name"], test_case["expected_failure"])
    print(f"Batch processing test result: {'PASSED' if success else 'FAILED'}")


if __name__ == "__main__":
    test_batch()