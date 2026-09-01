import pytest
from pathlib import Path
import shutil

import numpy as np

from test_pet_motion_correct_cli import _nifti_spacing, _read_nifti


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
                "--tracer",
                "abeta",
            ]
        )

        assert result.returncode == 0, (
            "adni-pet-core command failed.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )
        assert output_path.exists(), "adni-pet-core command did not create the expected output file."
        assert "4D PET detected" not in result.stdout

    def test_manual_fov_iterative(self, run_subprocess, tmp_path, test_files):
        output_path = tmp_path / "adni_pet_core_iter_manual.nii"
        result = run_subprocess(
            [
                "adni-pet-core",
                "--input",
                str(test_files["input"]),
                "--output",
                str(output_path),
                "--tracer",
                "tau",
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

    def test_requires_tracer(self, run_subprocess, tmp_path, test_files):
        output_path = tmp_path / "adni_pet_core_missing_tracer.nii"
        result = run_subprocess(
            [
                "adni-pet-core",
                "--input",
                str(test_files["input"]),
                "--output",
                str(output_path),
            ]
        )

        assert result.returncode != 0
        assert "--tracer" in result.stderr
        assert not output_path.exists()

    def test_rejects_unknown_tracer(self, run_subprocess, tmp_path, test_files):
        output_path = tmp_path / "adni_pet_core_unknown_tracer.nii"
        result = run_subprocess(
            [
                "adni-pet-core",
                "--input",
                str(test_files["input"]),
                "--output",
                str(output_path),
                "--tracer",
                "unknown",
            ]
        )

        assert result.returncode != 0
        assert "allowed options: {abeta, tau, fdg, dat}" in result.stderr
        assert not output_path.exists()

    def test_fdg_uses_iterative_global_mean_normalization(
        self, run_subprocess, tmp_path, test_files
    ):
        output_path = tmp_path / "adni_pet_core_fdg.nii"
        result = run_subprocess(
            [
                "adni-pet-core",
                "--input",
                str(test_files["input"]),
                "--output",
                str(output_path),
                "--tracer",
                "fdg",
            ]
        )

        assert result.returncode == 0, result.stderr
        data, dimension = _read_nifti(output_path)
        assert dimension == 3
        retained = data[data >= 0.5]
        assert retained.size > 0
        assert np.count_nonzero((data > 0.0) & (data < 0.5)) > 0
        assert retained.mean() == pytest.approx(1.0, abs=1e-5)
        assert data.mean() < 0.5

    def test_dat_ppmi_levels_one_and_two_use_four_frame_input(
        self, run_subprocess, tmp_path, test_files
    ):
        output_path = tmp_path / "dat_Coreg_Avg.nii.gz"
        coreg_path = tmp_path / "dat_Coreg_Avg_Coreg.nii.gz"

        result = run_subprocess(
            [
                "adni-pet-core",
                "--input", str(test_files["four_frames"]),
                "--output", str(output_path),
                "--tracer", "dat",
                "--level", "1", "2",
            ]
        )

        assert result.returncode == 0, result.stderr
        _, coreg_dimension = _read_nifti(coreg_path)
        _, average_dimension = _read_nifti(output_path)
        assert coreg_dimension == 4
        assert average_dimension == 3
        assert "stopped before Level 3" in result.stdout

    def test_dat_ppmi_level_three_uses_occipital_reference(
        self, run_subprocess, tmp_path, test_files
    ):
        output_path = tmp_path / "dat_ppmi.nii.gz"
        result = run_subprocess(
            [
                "adni-pet-core",
                "--input", str(test_files["four_frames"]),
                "--output", str(output_path),
                "--tracer", "dat",
                "--level", "3",
                "--config", str(test_files["config"]),
            ]
        )

        assert result.returncode == 0, result.stderr
        data, dimension = _read_nifti(output_path)
        occipital_mask, mask_dimension = _read_nifti(
            test_files["dat_occipital_ref"]
        )
        assert dimension == mask_dimension == 3
        assert occipital_mask.shape == (91, 109, 91)
        assert _nifti_spacing(test_files["dat_occipital_ref"]) == (
            2.0,
            2.0,
            2.0,
        )
        assert (
            'dat_occipital_ref = "assets/nii/DAT/DAT_Occipital_Ref.nii"'
            in test_files["config"].read_text()
        )
        assert np.count_nonzero(occipital_mask == 1) > 0
        assert np.isfinite(data).all()
        assert np.count_nonzero(data > 0.0) > 0

    def test_deface_applies_to_dynamic_coreg_and_average(
        self, run_subprocess, tmp_path, test_files
    ):
        output_path = tmp_path / "defaced_average.nii.gz"
        coreg_path = tmp_path / "defaced_average_Coreg.nii.gz"

        result = run_subprocess(
            [
                "adni-pet-core",
                "--input", str(test_files["four_frames"]),
                "--output", str(output_path),
                "--tracer", "dat",
                "--level", "1", "2",
                "--deface",
                "--config", str(test_files["config"]),
            ]
        )

        assert result.returncode == 0, result.stderr
        corrected, corrected_dimension = _read_nifti(coreg_path)
        averaged, averaged_dimension = _read_nifti(output_path)
        assert corrected_dimension == 4
        assert averaged_dimension == 3
        np.testing.assert_allclose(
            averaged, corrected.mean(axis=0), rtol=1e-5, atol=1e-6
        )
        assert np.count_nonzero(np.all(corrected == 0.0, axis=0)) > 0
        assert "Level 1 Coreg defaced and saved" in result.stdout
        assert "Level 2 Coreg, Avg defaced and saved" in result.stdout

    def test_batch_mode_outputs(self, run_subprocess, tmp_path, test_files):
        input_dir = tmp_path / "adni_pet_core_batch_inputs"
        output_dir = tmp_path / "adni_pet_core_batch_outputs"
        input_dir.mkdir()
        output_dir.mkdir()

        shutil.copy(test_files["input"], input_dir / "sample_adni_pet_core.nii")

        result = run_subprocess(
            [
                "adni-pet-core",
                "--input",
                str(input_dir),
                "--output",
                str(output_dir),
                "--batch",
                "--tracer",
                "abeta",
            ]
        )

        assert result.returncode == 0, (
            "adni-pet-core batch command failed.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )

        generated = list(output_dir.glob("*.nii"))
        assert generated, "adni-pet-core batch run did not produce any output files."
        assert (output_dir / "batch_info.txt").exists(), "adni-pet-core batch run missing batch_info.txt"

    def test_batch_mode_saves_only_selected_level(self, run_subprocess, tmp_path, test_files):
        input_dir = tmp_path / "adni_level_batch_inputs"
        output_dir = tmp_path / "adni_level_batch_outputs"
        input_dir.mkdir()
        output_dir.mkdir()
        shutil.copy(test_files["input"], input_dir / "sample.nii")

        result = run_subprocess(
            [
                "adni-pet-core",
                "--input", str(input_dir),
                "--output", str(output_dir),
                "--batch",
                "--tracer", "abeta",
                "--level", "2",
                "--config", str(tmp_path / "does-not-exist.toml"),
            ]
        )

        assert result.returncode == 0, result.stderr
        assert (output_dir / "sample_Coreg_Avg.nii").exists()
        assert not (output_dir / "sample_Coreg.nii").exists()
        assert not (output_dir / "sample_ADNI_style.nii").exists()
