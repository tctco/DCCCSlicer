import sys
import types
from pathlib import Path


def _load_metric_calculator_logic():
    # Stub external runtime dependencies unavailable in CI unit-test environment.
    if "qt" not in sys.modules:
        class _DummyQProcess:
            NormalExit = 0

        sys.modules["qt"] = types.SimpleNamespace(
            QProcess=_DummyQProcess,
            QProcessEnvironment=object,
        )
    if "slicer" not in sys.modules:
        sys.modules["slicer"] = types.SimpleNamespace(util=types.SimpleNamespace())

    repo_root = Path(__file__).resolve().parents[3]
    sys.path.insert(0, str(repo_root / "localizer" / "lib"))
    from metric_calculator import MetricCalculatorLogic

    return MetricCalculatorLogic


MetricCalculatorLogic = _load_metric_calculator_logic()


def test_build_command_uses_subcommand_cli_for_centaur():
    logic = MetricCalculatorLogic("/tmp/plugin")

    cmd = logic._build_command(
        metric_type="CenTauR",
        input_path="in.nii",
        output_path="out.nii",
        algorithm_style="SPM style",
        manual_fov=True,
        iterative=True,
        skip_normalization=True,
    )

    assert cmd[:2] == [str(logic.executable_path), "centaur"]
    assert "--input" in cmd and "--output" in cmd
    assert "--manual-fov" in cmd
    assert "--manual-fov-placement" not in cmd
    assert "--skip-normalization" in cmd


def test_build_command_fillstates_requires_tracer_and_no_suvr():
    logic = MetricCalculatorLogic("/tmp/plugin")

    cmd = logic._build_command(
        metric_type="Fill States",
        input_path="in.nii",
        output_path="out.nii",
        algorithm_style="SPM style",
        manual_fov=False,
        iterative=False,
        skip_normalization=False,
        tracer="ftp",
    )

    assert cmd[1] == "fillstates"
    assert "--tracer" in cmd
    assert "ftp" in cmd
    assert "--suvr" not in cmd


def test_build_suvr_command_does_not_reference_legacy_flag():
    logic = MetricCalculatorLogic("/tmp/plugin")

    cmd = logic._build_suvr_command(
        input_path="in.nii",
        output_path="out.nii",
        roi_path="roi.nii",
        ref_path="ref.nii",
        algorithm_style="SPM style",
        manual_fov=True,
        iterative=False,
        skip_normalization=True,
    )

    assert cmd[:2] == [str(logic.executable_path), "suvr"]
    assert "--manual-fov" in cmd
    assert "--skip-normalization" in cmd
