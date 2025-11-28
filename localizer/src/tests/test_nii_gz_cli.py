import pytest
from pathlib import Path


TEST_DIR = Path(__file__).parent.resolve()


@pytest.fixture(scope="session")
def nii_gz_file():
    """Fixture that points to the .nii.gz test image."""
    path = TEST_DIR / "test_nii_gz" / "fs_AV45_mean.nii.gz"
    if not path.exists():
        pytest.skip(f"Required test file missing: {path}")
    return path


class TestNiiGzSupport:
    """
    Tests to ensure the CLI can accept .nii.gz input files.
    """

    @pytest.mark.parametrize("subcommand", ["normalize", "centiloid"])
    def test_cli_accepts_nii_gz_input(
        self,
        subcommand: str,
        nii_gz_file: Path,
        run_subprocess,
        tmp_path,
    ):
        """
        Run selected subcommands with a .nii.gz input and ensure they succeed.
        """
        output_path = tmp_path / f"output_{subcommand}.nii"

        args = [
            subcommand,
            "--input",
            str(nii_gz_file),
            "--output",
            str(output_path),
        ]

        # For SUVr-derived metrics like centiloid, a tracer argument is required
        if subcommand == "centiloid":
            args.extend(["--tracer", "fbp"])

        result = run_subprocess(args)

        assert result.returncode == 0, (
            f"{subcommand} command failed for .nii.gz input.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )


