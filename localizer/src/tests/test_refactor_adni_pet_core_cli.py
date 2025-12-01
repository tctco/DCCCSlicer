import pytest


class TestRefactorAdniPetCoreCLI:
    """Tests for the refactored ADNI PET Core normalize command."""

    def test_adni_pet_core_basic(self, run_subprocess, tmp_path, test_files):
        """
        Ensure the ADNI PET Core flow runs end-to-end with default flags.
        """
        output_path = tmp_path / "adni_pet_core_output.nii"
        args = [
            "adni-pet-core",
            "--input",
            str(test_files["input"]),
            "--output",
            str(output_path),
        ]

        result = run_subprocess(args)

        assert result.returncode == 0, (
            "adni-pet-core command failed.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )
        assert output_path.exists(), "adni-pet-core command did not create the expected output file."

    def test_adni_pet_core_manual_fov_iterative(self, run_subprocess, tmp_path, test_files):
        """
        Verify manual FOV and iterative flags are wired for the ADNI variant.
        """
        output_path = tmp_path / "adni_pet_core_iter_manual.nii"
        args = [
            "adni-pet-core",
            "--input",
            str(test_files["input"]),
            "--output",
            str(output_path),
            "--iterative",
            "--manual-fov",
        ]

        result = run_subprocess(args)

        assert result.returncode == 0, (
            "adni-pet-core command with iterative/manual flags failed.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )
        assert output_path.exists(), "adni-pet-core iterative/manual run did not create the expected output file."

    def test_adni_pet_core_requires_input(self, run_subprocess, tmp_path):
        """
        Command should fail fast if required arguments are missing.
        """
        output_path = tmp_path / "adni_pet_core_missing_input.nii"
        args = [
            "adni-pet-core",
            "--output",
            str(output_path),
        ]

        result = run_subprocess(args)

        assert result.returncode != 0, (
            "adni-pet-core command should fail when --input is omitted.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )

