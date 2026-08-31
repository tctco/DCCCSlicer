class TestBrainExtractCLI:
    def test_basic(self, run_subprocess, tmp_path, test_files):
        output_path = tmp_path / "brain_extracted.nii.gz"
        mask_path = tmp_path / "native_brain_mask.nii.gz"

        result = run_subprocess(
            [
                "brain-extract",
                "--input",
                str(test_files["input"]),
                "--output",
                str(output_path),
                "--mask-output",
                str(mask_path),
            ]
        )

        assert result.returncode == 0, (
            "brain-extract command failed.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )
        assert output_path.exists()
        assert mask_path.exists()

    def test_rejects_batch_mode(self, run_subprocess, tmp_path, test_files):
        result = run_subprocess(
            [
                "brain-extract",
                "--input",
                str(test_files["input"]),
                "--output",
                str(tmp_path / "output.nii"),
                "--batch",
            ]
        )

        assert result.returncode != 0
        assert "not supported" in result.stderr
