from pathlib import Path

import pytest


SAMPLE_FILES = ["YC03_PET.nii", "ED02_PET.nii"]

# TODO: test on more samples and write a brief reproduction report


@pytest.mark.parametrize("filename", SAMPLE_FILES)
def test_abetaload_cli_on_samples(run_subprocess, tmp_path, test_files, filename):
    """
    Run abetaload CLI on provided PET samples and print decomposition outputs.
    """
    input_path = Path(test_files["test_dir"]) / "test_acc_centiloid_centaurz" / filename
    if not input_path.exists():
        pytest.skip(f"Missing test input: {input_path}")

    output_path = tmp_path / f"{filename}_abetaload.nii"
    args = [
        "abetaload",
        "--input",
        str(input_path),
        "--output",
        str(output_path),
    ]

    result = run_subprocess(args)

    print(f"\n[abetaload][{filename}] STDOUT:\n{result.stdout}")
    if result.stderr:
        print(f"\n[abetaload][{filename}] STDERR:\n{result.stderr}")

    assert result.returncode == 0, (
        f"abetaload command failed for {filename}.\n"
        f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
    )
    assert output_path.exists(), "Normalized output NIfTI was not created."
    assert "Abeta_load" in result.stdout, "Abeta_load value missing in CLI output."
