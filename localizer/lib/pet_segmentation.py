"""Native PET segmentation adapter; all MRML operations run on the GUI thread."""
import colorsys
import json
from pathlib import Path
import sys
import tempfile


class PetSegmentationLogic:
    def __init__(self, plugin_path):
        self.plugin_path = Path(plugin_path)
        self.executable = self.plugin_path / "cpp" / ("DCCCcore.exe" if sys.platform == "win32" else "DCCCcore")
        self.process = None
        self._work = None
        self._callback = None

    def command(self, input_path, output_path, *, mode="freesurfer", tta=True):
        if mode not in ("freesurfer", "training", "neurosegmenter"):
            raise ValueError("Unknown segmentation mode")
        args = [str(self.executable), "seg", "--input", str(input_path),
                "--output", str(output_path), "--mode", mode, "--threads", "4"]
        if not tta:
            args.append("--no-tta")
        return args

    def label_names(self, mode):
        bundle = self.plugin_path / "cpp/assets/models/seg"
        if mode == "neurosegmenter":
            mapping = json.loads((bundle / "neurosegmenter_mapping.json").read_text())
            return {0: "background", **{r["id"]: r["name"] for r in mapping["regions"] if r["available"]}}
        labels = json.loads((bundle / "labels.json").read_text())
        return {(labels["raw_ids"][i] if mode == "freesurfer" else i): name
                for name, i in labels["labels"].items()}

    def load_result(self, output_path, source_id, source_name, mode):
        import slicer
        labelmap = slicer.util.loadLabelVolume(str(output_path), {"name": source_name + "_seg_" + mode})
        if labelmap is None:
            raise RuntimeError("Unable to load segmentation labelmap")
        colors = None
        try:
            names = self.label_names(mode)
            colors = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLColorTableNode", source_name + "_seg_labels")
            colors.SetTypeToUser()
            colors.SetNumberOfColors(max(names) + 1)
            colors.NamesInitialisedOn()
            for i in range(max(names) + 1):
                r, g, b = colorsys.hsv_to_rgb((i * 0.61803398875) % 1, 0.65, 0.95)
                colors.SetColor(i, names.get(i, "unused"), r, g, b, 1.0 if i in names and i != 0 else 0.0)
            labelmap.GetDisplayNode().SetAndObserveColorNodeID(colors.GetID())
            source = slicer.mrmlScene.GetNodeByID(source_id)
            if source:
                slicer.util.setSliceViewerLayers(background=source, label=labelmap, labelOpacity=0.4)
            return labelmap
        except Exception:
            slicer.mrmlScene.RemoveNode(labelmap)
            if colors:
                slicer.mrmlScene.RemoveNode(colors)
            raise

    def segment_async(self, input_node, *, mode="freesurfer", tta=True, callback):
        import qt
        import slicer
        if self.process is not None:
            raise RuntimeError("A segmentation is already running")
        if not input_node or not input_node.IsA("vtkMRMLScalarVolumeNode"):
            raise ValueError("Select a PET scalar volume")
        if not self.executable.is_file():
            raise FileNotFoundError("Bundled DCCCcore executable is missing: " + str(self.executable))
        self.label_names(mode)  # Check resources before exporting the volume.
        self._work = tempfile.TemporaryDirectory(prefix="dccc_seg_")
        directory = Path(self._work.name)
        clone = None
        try:
            clone = slicer.modules.volumes.logic().CloneVolume(slicer.mrmlScene, input_node, "seg_export")
            clone.SetAndObserveTransformNodeID(input_node.GetTransformNodeID())
            if clone.GetParentTransformNode():
                if not slicer.vtkSlicerTransformLogic().hardenTransform(clone):
                    raise RuntimeError("Cannot harden the input volume's parent transform")
            if not slicer.util.saveNode(clone, str(directory / "pet.nii.gz")):
                raise RuntimeError("Cannot export PET volume")
            self._source_id, self._source_name, self._mode = input_node.GetID(), input_node.GetName(), mode
            command = self.command(directory / "pet.nii.gz", directory / "labels.nii.gz", mode=mode, tta=tta)
            self._callback = callback
            self.process = qt.QProcess()
            self.process.setWorkingDirectory(str(self.executable.parent))
            self.process.connect("finished(int,QProcess::ExitStatus)", self._finished)
            self.process.connect("errorOccurred(QProcess::ProcessError)", self._process_error)
            self.process.start(command[0], command[1:])
        except Exception:
            self.cancel()
            raise
        finally:
            if clone:
                slicer.mrmlScene.RemoveNode(clone)

    def _process_error(self, error):
        if self.process is not None:
            self._finish(False, self.process.errorString())

    def _finished(self, code, status):
        if self.process is None:
            return
        if code != 0 or int(status) != 0:
            error = bytes(self.process.readAllStandardError()).decode("utf-8", errors="replace")
            output = bytes(self.process.readAllStandardOutput()).decode("utf-8", errors="replace")
            self._finish(False, error or output or "Segmentation process failed")
            return
        try:
            labelmap = self.load_result(Path(self._work.name) / "labels.nii.gz", self._source_id, self._source_name, self._mode)
        except Exception as exc:
            self._finish(False, str(exc))
            return
        self._finish(True, "", labelmap)

    def _finish(self, success, error, labelmap=None):
        callback = self._callback
        self.cancel()
        if callback:
            callback(success, labelmap, error)

    def cancel(self):
        self._callback = None
        if self.process is not None:
            process, self.process = self.process, None
            if process.state() != 0:
                process.kill()
                process.waitForFinished(1000)
            process.deleteLater()
        if self._work:
            self._work.cleanup()
            self._work = None
