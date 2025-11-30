import pytest
from pathlib import Path


def _with_suffix(base: Path, suffix: str) -> Path:
    return base.with_name(f"{base.stem}{suffix}{base.suffix}")


def _expected_output_paths(base: Path):
    suffixes = ["", "_stripped_image", "_stripped_component", "_AD_prob_map"]
    return [_with_suffix(base, suffix) for suffix in suffixes]


class TestRefactorADADCLI:
    """Tests for the refactor-adad prototype command."""

    @pytest.mark.parametrize("modality", ["abeta", "tau"])
    def test_refactor_adad_generates_all_outputs(self, run_subprocess, tmp_path, test_files, modality):
        """
        Ensure the refactored ADAD pipeline finishes successfully and saves every expected file.
        """
        output_path = tmp_path / f"refactor_adad_{modality}.nii"
        args = [
            "refactor-adad",
            "--input",
            str(test_files["input"]),
            "--output",
            str(output_path),
            "--modality",
            modality,
        ]

        result = run_subprocess(args)

        assert result.returncode == 0, (
            f"refactor-adad command failed for modality={modality}.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )

        for path in _expected_output_paths(output_path):
            assert path.exists(), f"Expected output file missing: {path}"

    def test_refactor_adad_skip_normalization_outputs(self, run_subprocess, tmp_path, test_files):
        """
        Ensure the command honors --skip-normalization yet still writes all ADAD artifacts.
        """
        output_path = tmp_path / "refactor_adad_skip.nii"
        args = [
            "refactor-adad",
            "--input",
            str(test_files["input"]),
            "--output",
            str(output_path),
            "--skip-normalization",
        ]

        result = run_subprocess(args)

        assert result.returncode == 0, (
            "refactor-adad command failed with --skip-normalization.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )

        for path in _expected_output_paths(output_path):
            assert path.exists(), f"Expected output file missing: {path}"


