import pytest


class TestRefactorFillStatesCLI:
    """Tests for the refactor-fillstates prototype command."""

    @pytest.mark.parametrize("tracer", ["fbp", "fdg", "ftp"])
    def test_refactor_fillstates_basic(self, run_subprocess, tmp_path, test_files, tracer):
        """
        Ensure the refactored FillStates pipeline completes for each tracer.
        """
        output_path = tmp_path / f"refactor_fillstates_{tracer}.nii"
        args = [
            "refactor-fillstates",
            "--input",
            str(test_files["input"]),
            "--output",
            str(output_path),
            "--tracer",
            tracer,
        ]

        result = run_subprocess(args)

        assert result.returncode == 0, (
            f"refactor-fillstates failed for tracer={tracer}.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )

    def test_refactor_fillstates_skip_normalization(self, run_subprocess, tmp_path, test_files):
        """
        Ensure the command honors --skip-normalization when input is pre-normalized.
        """
        output_path = tmp_path / "refactor_fillstates_skip.nii"
        args = [
            "refactor-fillstates",
            "--input",
            str(test_files["input"]),
            "--output",
            str(output_path),
            "--tracer",
            "fbp",
            "--skip-normalization",
        ]

        result = run_subprocess(args)

        assert result.returncode == 0, (
            "refactor-fillstates command failed with --skip-normalization.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )


