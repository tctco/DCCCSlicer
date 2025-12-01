import pytest


class TestRefactorSUVrCLI:
    """Tests for the suvr prototype command."""

    def test_refactor_suvr_basic(self, run_subprocess, tmp_path, test_files):
        """
        Ensure the refactored SUVR pipeline runs end-to-end with provided masks.
        """
        output_path = tmp_path / "refactor_suvr_output.nii"
        args = [
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

        result = run_subprocess(args)

        assert result.returncode == 0, (
            "suvr command failed.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )

    def test_refactor_suvr_requires_masks(self, run_subprocess, tmp_path, test_files):
        """
        Ensure the command fails when VOI/Ref masks are not supplied.
        """
        output_path = tmp_path / "refactor_suvr_missing_masks.nii"
        args = [
            "suvr",
            "--input",
            str(test_files["input"]),
            "--output",
            str(output_path),
        ]

        result = run_subprocess(args)

        assert result.returncode != 0, (
            "suvr command should fail when masks are omitted.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )

