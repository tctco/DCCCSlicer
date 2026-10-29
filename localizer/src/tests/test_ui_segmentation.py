"""Boundary tests for Slicer PET segmentation without a Slicer installation."""
import importlib.util
import json
from pathlib import Path
import sys
import types

import pytest


def load_adapter():
    path = Path(__file__).resolve().parents[2] / "lib" / "pet_segmentation.py"
    spec = importlib.util.spec_from_file_location("pet_segmentation", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.PetSegmentationLogic


def test_seg_command_preserves_path_arguments_without_flip_options(tmp_path):
    logic = load_adapter()(tmp_path)
    command = logic.command("input pet.nii.gz", "output labels.nii.gz")
    assert command[1:] == ["seg", "--input", "input pet.nii.gz", "--output",
                           "output labels.nii.gz", "--mode", "freesurfer", "--threads", "4"]
    command = logic.command("in", "out", mode="training")
    assert command[command.index("--mode") + 1] == "training"
    with pytest.raises(ValueError):
        logic.command("in", "out", mode="wrong")


@pytest.mark.parametrize("tta", [True, False])
def test_seg_adapter_rejects_legacy_tta_keyword(tmp_path, monkeypatch, tta):
    logic = load_adapter()(tmp_path)
    with pytest.raises(TypeError, match="tta"):
        logic.command("in", "out", tta=tta)
    monkeypatch.setitem(sys.modules, "qt", types.SimpleNamespace())
    monkeypatch.setitem(sys.modules, "slicer", types.SimpleNamespace())
    with pytest.raises(TypeError, match="tta"):
        logic.segment_async(None, tta=tta, callback=lambda *args: None)


def test_seg_ui_has_no_flip_control():
    import xml.etree.ElementTree as ET
    path = Path(__file__).resolve().parents[2] / "Resources/UI/localizer.ui"
    ui = ET.parse(path)
    controls = ui.findall(".//widget[@class='QCheckBox']")
    assert not any("tta" in c.attrib.get("name", "").lower() or
                   "flip" in c.attrib.get("name", "").lower() for c in controls)


def test_label_names_are_mode_specific_and_pons_unavailable(tmp_path):
    logic = load_adapter()(tmp_path)
    bundle = tmp_path / "cpp/assets/models/seg"
    bundle.mkdir(parents=True)
    (bundle / "labels.json").write_text(json.dumps({"raw_ids": [0, 17, 53],
        "labels": {"background": 0, "Left-Hippocampus": 1, "Right-Hippocampus": 2}}))
    (bundle / "neurosegmenter_mapping.json").write_text(json.dumps({"regions": [
        {"id": 47, "name": "hippocampus_left", "available": True},
        {"id": 46, "name": "pons", "available": False}]}))
    assert logic.label_names("freesurfer")[17] == "Left-Hippocampus"
    assert logic.label_names("training")[2] == "Right-Hippocampus"
    assert logic.label_names("neurosegmenter") == {0: "background", 47: "hippocampus_left"}


def test_result_is_loaded_as_named_labelmap(monkeypatch, tmp_path):
    logic = load_adapter()(tmp_path)
    monkeypatch.setattr(logic, "label_names", lambda mode: {0: "background", 17: "Left-Hippocampus"})
    calls = {}
    class Colors:
        def SetTypeToUser(self): pass
        def SetNumberOfColors(self, n): calls["color_count"] = n
        def NamesInitialisedOn(self): pass
        def SetColor(self, i, name, *rgba): calls[i] = (name, rgba)
        def GetID(self): return "colors"
    display = types.SimpleNamespace(SetAndObserveColorNodeID=lambda value: calls.update(color_id=value))
    labelmap = types.SimpleNamespace(GetDisplayNode=lambda: display)
    source = object()
    def load(path, properties):
        calls["path"], calls["name"] = path, properties["name"]
        return labelmap
    fake = types.SimpleNamespace(util=types.SimpleNamespace(loadLabelVolume=load,
        setSliceViewerLayers=lambda **kwargs: calls.update(layers=kwargs)),
        mrmlScene=types.SimpleNamespace(AddNewNodeByClass=lambda *args: Colors(),
            GetNodeByID=lambda value: source, RemoveNode=lambda node: None))
    monkeypatch.setitem(sys.modules, "slicer", fake)
    assert logic.load_result(tmp_path / "labels.nii.gz", "source", "PET", "freesurfer") is labelmap
    assert calls["name"] == "PET_seg_freesurfer"
    assert calls[17][0] == "Left-Hippocampus"
    assert calls[0][1][-1] == 0
    assert calls["color_id"] == "colors"
    assert calls["layers"]["background"] is source
    assert calls["layers"]["label"] is labelmap


def test_process_failure_does_not_load_result(monkeypatch, tmp_path):
    logic = load_adapter()(tmp_path)
    events = []
    logic.process = types.SimpleNamespace(readAllStandardError=lambda: b"invalid model",
        readAllStandardOutput=lambda: b"", state=lambda: 0, deleteLater=lambda: None)
    logic._callback = lambda success, node, error: events.append((success, node, error))
    logic._finished(1, 0)
    assert events == [(False, None, "invalid model")]
    assert logic.process is None
