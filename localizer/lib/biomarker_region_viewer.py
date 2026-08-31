"""Load and display the bundled biomarker masks in 3D Slicer."""

from pathlib import Path

import slicer


class BiomarkerRegionViewer:
    """Resolve biomarker assets and present them in the slice viewers."""

    MNI_ASSET = "padded_MNI152_T1_2mm.nii"
    REGION_ASSETS = {
        "Centiloid": {
            "voi": "voi_ctx_2mm.nii",
            "ref": "voi_WhlCbl_2mm.nii",
        },
        "CenTauR": {
            "voi": "CenTauR.nii",
            "ref": "voi_CerebGry_tau_2mm.nii",
        },
        "CenTauRz": {
            "voi": "CenTauR.nii",
            "ref": "voi_CerebGry_tau_2mm.nii",
        },
    }
    FILL_STATES_VOI_ASSETS = {
        "FBP": "fill_states/fs_FBP_meta_roi.nii",
        "FDG": "fill_states/fs_FDG_meta_roi.nii",
        "FTP": "CenTauR.nii",
    }

    def __init__(self, plugin_path):
        self.plugin_path = Path(plugin_path)
        self._loaded_nodes = {}

    def asset_relative_path(self, region_type, metric_type=None, tracer=None):
        """Return an asset path relative to the bundled ``assets/nii`` folder."""

        region_type = region_type.lower()
        if region_type == "mni":
            return self.MNI_ASSET
        if metric_type == "Fill States":
            if region_type != "voi":
                return None
            return self.FILL_STATES_VOI_ASSETS.get((tracer or "").upper())
        return self.REGION_ASSETS.get(metric_type, {}).get(region_type)

    def resolve_asset_path(self, relative_path):
        """Resolve an asset in an installed extension or a source checkout."""

        if not relative_path:
            return None
        candidates = (
            self.plugin_path / "cpp" / "assets" / "nii" / relative_path,
            self.plugin_path / "src" / "assets" / "nii" / relative_path,
        )
        return next((path for path in candidates if path.is_file()), None)

    def show_region(self, region_type, metric_type=None, tracer=None):
        """Show MNI152, or overlay a selected biomarker mask on MNI152."""

        relative_path = self.asset_relative_path(region_type, metric_type, tracer)
        if not relative_path:
            slicer.util.warningDisplay(
                f"No {region_type.upper()} region is defined for {metric_type or 'this biomarker'}."
            )
            return None

        asset_path = self.resolve_asset_path(relative_path)
        if asset_path is None:
            slicer.util.errorDisplay(f"Bundled brain-region asset was not found: {relative_path}")
            return None

        node_name = self._node_name(region_type, metric_type, tracer)
        region_node = self._load_once(asset_path, node_name)
        if region_node is None:
            return None

        if region_type.lower() == "mni":
            slicer.util.setSliceViewerLayers(background=region_node, foreground="")
        else:
            mni_path = self.resolve_asset_path(self.MNI_ASSET)
            if mni_path is None:
                slicer.util.errorDisplay(
                    f"Bundled MNI152 template was not found: {self.MNI_ASSET}"
                )
                return None
            mni_node = self._load_once(mni_path, "MNI152_2mm")
            self._configure_mask_display(region_node, region_type)
            slicer.util.setSliceViewerLayers(
                background=mni_node,
                foreground=region_node,
                foregroundOpacity=0.5,
            )

        slicer.app.applicationLogic().FitSliceToAll()
        return region_node

    def _load_once(self, path, node_name):
        cache_key = str(path)
        node = self._loaded_nodes.get(cache_key)
        if node is not None and node.GetScene() is not None:
            return node

        node = slicer.util.loadVolume(
            str(path), properties={"name": node_name, "show": False}
        )
        if not node:
            slicer.util.errorDisplay(f"Failed to load brain-region asset: {path}")
            return None
        self._loaded_nodes[cache_key] = node
        return node

    @staticmethod
    def _configure_mask_display(node, region_type):
        if node.GetDisplayNode() is None:
            node.CreateDefaultDisplayNodes()
        display_node = node.GetDisplayNode()
        color_node_id = (
            "vtkMRMLColorTableNodeRed"
            if region_type.lower() == "voi"
            else "vtkMRMLColorTableNodeBlue"
        )
        display_node.SetAndObserveColorNodeID(color_node_id)
        display_node.SetOpacity(0.5)
        display_node.SetVisibility(True)

    @staticmethod
    def _node_name(region_type, metric_type, tracer):
        if region_type.lower() == "mni":
            return "MNI152_2mm"
        suffix = tracer.upper() if metric_type == "Fill States" and tracer else metric_type
        return f"{suffix}_{region_type.upper()}"
