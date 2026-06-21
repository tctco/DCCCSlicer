class TestSUVRCLI:
    """SUVR-specific checks."""

    def test_basic(self, run_subprocess, tmp_path, test_files):
        output_path = tmp_path / "suvr_output.nii"
        result = run_subprocess(
            [
                "suvr",
                "--input",
                str(test_files["input"]),
                "--output",
                str(output_path),
                "--voi-mask",
                str(test_files["voi_mask"]),
                "--ref-mask",
                str(test_files["ref_mask"]),
            ]
        )

        assert result.returncode == 0, (
            "suvr command failed.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )

    def test_requires_masks(self, run_subprocess, tmp_path, test_files):
        output_path = tmp_path / "suvr_missing_masks.nii"
        result = run_subprocess(
            [
                "suvr",
                "--input",
                str(test_files["input"]),
                "--output",
                str(output_path),
            ]
        )

        assert result.returncode != 0, (
            "suvr command should fail when masks are omitted.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )
