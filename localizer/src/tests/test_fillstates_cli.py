import pytest
from pathlib import Path


class TestFillStatesCLI:
    """Tests for the fillstates CLI subcommand."""

    @pytest.mark.parametrize("tracer", ["fbp", "fdg", "ftp"])
    def test_fillstates_basic(self, run_subprocess, tmp_path, test_files, tracer):
        """
        Ensure fillstates runs successfully for each supported tracer.
        """
        output_path = tmp_path / f"output_fillstates_{tracer}.nii"

        args = [
            "fillstates",
            "--input",
            str(test_files["input"]),
            "--output",
            str(output_path),
            "--tracer",
            tracer,
        ]

        result = run_subprocess(args)

        assert result.returncode == 0, (
            f"fillstates failed for tracer={tracer}.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )
    
    @pytest.mark.parametrize("tracer", ['non-existent-tracer', 'fbb'])
    def test_fillstates_cli_non_existent_tracer(self, run_subprocess, tmp_path, test_files, tracer):
        """
        Ensure fillstates fails for non-existent tracer.
        """
        output_path = tmp_path / f"output_fillstates_{tracer}.nii"
        args = [
            "fillstates",
            "--input",
            str(test_files["input"]),
            "--output",
            str(output_path),
            "--tracer",
            tracer,
        ]
        result = run_subprocess(args)
        assert result.returncode != 0, (
            f"fillstates should have failed for tracer={tracer}.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )
    
    @pytest.mark.parametrize("tracer", ['fbp'])
    def test_fillstates_cli_unsupported_configuration(self, run_subprocess, tmp_path, test_files, tracer):
        """
        Ensure fillstates fails for unsupported configuration.
        """
        output_path = tmp_path / f"output_fillstates_{tracer}.nii"
        args = [
            "fillstates",
            "--input",
            str(test_files["input"]),
            "--output",
            str(output_path),
            "--tracer",
            tracer,
            "--config",
            'config.fast_and_acc.toml',
        ]
        result = run_subprocess(args)
        assert result.returncode != 0, (
            f"fillstates should have failed for tracer={tracer}.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )


