from pathlib import Path

import pytest


SAMPLE_FILES = ["YC03_PET.nii", "ED02_PET.nii"]


@pytest.mark.parametrize("filename", SAMPLE_FILES)
def test_abetaindex_cli_on_samples(run_subprocess, tmp_path, test_files, filename):
    """
    Run abetaindex CLI on provided PET samples and print the computed coefficient.
    """
    input_path = Path(test_files["test_dir"]) / "test_acc_centiloid_centaurz" / filename
    if not input_path.exists():
        pytest.skip(f"Missing test input: {input_path}")

    output_path = tmp_path / f"{filename}_abetaindex.nii"
    args = [
        "abetaindex",
        "--input",
        str(input_path),
        "--output",
        str(output_path),
    ]

    result = run_subprocess(args)

    print(f"\n[abetaindex][{filename}] STDOUT:\n{result.stdout}")
    if result.stderr:
        print(f"\n[abetaindex][{filename}] STDERR:\n{result.stderr}")

    assert result.returncode == 0, (
        f"abetaindex command failed for {filename}.\n"
        f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
    )
    assert output_path.exists(), "Normalized output NIfTI was not created."
    assert "AbetaIndex" in result.stdout, "AbetaIndex value missing in CLI output."
