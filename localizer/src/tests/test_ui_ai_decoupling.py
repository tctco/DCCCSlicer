import importlib
import sys
import types
from pathlib import Path


def _load_ai_decoupling_logic():
    if "slicer" not in sys.modules:
        sys.modules["slicer"] = types.SimpleNamespace(
            util=types.SimpleNamespace(saveNode=lambda *_args: True)
        )
    else:
        slicer_module = sys.modules["slicer"]
        if not hasattr(slicer_module, "util"):
            slicer_module.util = types.SimpleNamespace()
        slicer_module.util.saveNode = lambda *_args: True

    repo_root = Path(__file__).resolve().parents[3]
    sys.path.insert(0, str(repo_root / "localizer" / "lib"))
    module = importlib.import_module("ai_decoupling")
    return module, module.AIDecouplingLogic


def test_decouple_image_uses_adad_subcommand(monkeypatch):
    module, logic_type = _load_ai_decoupling_logic()
    logic = logic_type("/tmp/plugin")
    captured = {}

    def fake_run(command, **kwargs):
        captured["command"] = command
        captured["kwargs"] = kwargs
        return types.SimpleNamespace(returncode=0, stdout="ADAD score: 1", stderr="")

    monkeypatch.setattr(module.subprocess, "run", fake_run)
    monkeypatch.setattr(
        logic,
        "_load_result_volumes",
        lambda: (object(), object(), object()),
    )

    input_node = types.SimpleNamespace(GetName=lambda: "PET")
    success, *_ = logic.decouple_image(input_node, "Abeta")

    assert success
    assert captured["command"][:2] == [str(logic.executable_path), "adad"]
    assert captured["command"][-2:] == ["--modality", "abeta"]
    assert captured["kwargs"]["cwd"] == Path("/tmp/plugin")
