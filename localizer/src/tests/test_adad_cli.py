import pytest
from pathlib import Path


def _expected_adad_outputs(base: Path):
    suffixes = ["", "_stripped_image", "_stripped_component", "_AD_prob_map"]
    return [_with_suffix(base, suffix) for suffix in suffixes]

def _with_suffix(base: Path, suffix: str) -> Path:
    return base.with_name(f"{base.stem}{suffix}{base.suffix}")



class TestADADCLI:
    """Targeted coverage for the ADAD pipeline."""

    @pytest.mark.parametrize("modality", ["abeta", "tau"])
    def test_generates_all_outputs(self, modality, run_subprocess, tmp_path, test_files):
        output_path = tmp_path / f"adad_{modality}.nii"
        result = run_subprocess(
            [
                "adad",
                "--input",
                str(test_files["input"]),
                "--output",
                str(output_path),
                "--modality",
                modality,
            ]
        )

        assert result.returncode == 0, (
            f"adad command failed for modality={modality}.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )

        for path in _expected_adad_outputs(output_path):
            assert path.exists(), f"Expected output file missing: {path}"

    def test_skip_normalization(self, run_subprocess, tmp_path, test_files):
        output_path = tmp_path / "adad_skip.nii"
        result = run_subprocess(
            [
                "adad",
                "--input",
                str(test_files["input"]),
                "--output",
                str(output_path),
                "--skip-normalization",
            ]
        )

        assert result.returncode == 0, (
            "adad command failed with --skip-normalization.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )

        for path in _expected_adad_outputs(output_path):
            assert path.exists(), f"Expected output file missing: {path}"


class TestAdniPetCoreCLI:
    """ADNI PET Core normalization coverage."""

    def test_basic(self, run_subprocess, tmp_path, test_files):
        output_path = tmp_path / "adni_pet_core_output.nii"
        result = run_subprocess(
            [
                "adni-pet-core",
                "--input",
                str(test_files["input"]),
                "--output",
                str(output_path),
            ]
        )

        assert result.returncode == 0, (
            "adni-pet-core command failed.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )
        assert output_path.exists(), "adni-pet-core command did not create the expected output file."

    def test_manual_fov_iterative(self, run_subprocess, tmp_path, test_files):
        output_path = tmp_path / "adni_pet_core_iter_manual.nii"
        result = run_subprocess(
            [
                "adni-pet-core",
                "--input",
                str(test_files["input"]),
                "--output",
                str(output_path),
                "--iterative",
                "--manual-fov",
            ]
        )

        assert result.returncode == 0, (
            "adni-pet-core command with iterative/manual flags failed.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )
        assert output_path.exists(), "adni-pet-core iterative/manual run did not create the expected output file."

    def test_requires_input(self, run_subprocess, tmp_path):
        output_path = tmp_path / "adni_pet_core_missing_input.nii"
        result = run_subprocess(
            [
                "adni-pet-core",
                "--output",
                str(output_path),
            ]
        )

        assert result.returncode != 0, (
            "adni-pet-core command should fail when --input is omitted.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )