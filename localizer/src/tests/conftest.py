import pytest
import sys
import subprocess
from pathlib import Path
import shutil

# Import the global results list so tests can append to it
try:
    from .conftest_plugins import ACC_TEST_RESULTS, pytest_terminal_summary
except ImportError:
    # Fallback if conftest_plugins is not found (should not happen in valid structure)
    ACC_TEST_RESULTS = []
    def pytest_terminal_summary(terminalreporter, exitstatus, config):
        pass

# Common constants
TEST_DIR = Path(__file__).parent.resolve()

@pytest.fixture(scope="session")
def exe_path():
    """
    Fixture to locate the CentiloidCalculator executable.
    """
    # User suggested: exe_path = TEST_DIR / '../install/bin/CentiloidCalculator'
    # We handle Windows extension automatically
    base_path = TEST_DIR / '../install/bin/CentiloidCalculator'
    
    if sys.platform == 'win32':
        exe = base_path.with_suffix('.exe')
    else:
        exe = base_path

    # Check if it exists, otherwise try to find it in other common locations (optional, based on quick_test logic)
    if not exe.exists():
        # Fallback for development environments mentioned in quick_test.py
        raise FileNotFoundError(f"Executable not found. Expected at: {base_path}")
    
    return str(exe.resolve())

@pytest.fixture(scope="session")
def data_dir():
    """Fixture for the inputs directory."""
    path = TEST_DIR / "test_batch"
    if not path.exists():
        pytest.skip(f"Required test files missing: {path}")
    return path

@pytest.fixture
def output_dir(tmp_path):
    """
    Fixture for the outputs directory using pytest's tmp_path.
    Returns a Path object to a temporary directory that is unique to each test execution.
    """
    path = tmp_path / "outputs"
    path.mkdir(exist_ok=True)
    return path

@pytest.fixture
def run_subprocess(exe_path):
    """Helper fixture to run the executable."""
    def _run(args, capture_output=True):
        cmd = [exe_path] + args
        print(f"Running: {' '.join(cmd)}")
        return subprocess.run(
            cmd,
            capture_output=capture_output,
            text=True,
            check=False # We handle return codes in tests
        )
    return _run


@pytest.fixture(scope="session")
def test_files():
    """Fixture that exposes common test files."""
    files = {
        "test_dir": TEST_DIR,
        "input": TEST_DIR / "test_batch" / "input.nii",
        "voi_mask": TEST_DIR / "voi_mask.nii",
        "ref_mask": TEST_DIR / "ref_mask.nii",
    }

    # missing = [name for name, path in files.items() if name != "test_dir" and not path.exists()]
    # if missing:
    #    pytest.skip(f"Required test files missing: {', '.join(missing)}")

    return files

