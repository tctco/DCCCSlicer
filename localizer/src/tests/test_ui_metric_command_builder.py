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


def test_bundled_core_executable_matches_packaged_name():
    logic = MetricCalculatorLogic("/tmp/plugin")
    executable_name = "DCCCcore.exe" if sys.platform == "win32" else "DCCCcore"

    assert logic.executable_path == Path("/tmp/plugin/cpp") / executable_name


def test_bundled_core_executable_uses_exe_suffix_on_windows():
    from core_executable import dccccore_executable_path

    assert dccccore_executable_path(
        "/tmp/plugin", platform="win32"
    ) == Path("/tmp/plugin/cpp/DCCCcore.exe")


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


def test_macos_security_hint_mentions_quarantine_command(monkeypatch):
    from macos_security import append_macos_security_hint

    monkeypatch.setattr(sys, "platform", "darwin")

    message = append_macos_security_hint(
        "Operation not permitted",
        "/tmp/plugin/cpp/DCCCcore",
        returncode=126,
    )

    assert "macOS may have blocked DCCCcore" in message
    assert "xattr -dr com.apple.quarantine" in message
    assert "/tmp/plugin/cpp/DCCCcore" in message
