import importlib
import sys
import types
from pathlib import Path


def _load_viewer_type():
    if "slicer" not in sys.modules:
        sys.modules["slicer"] = types.SimpleNamespace(util=types.SimpleNamespace())
    repo_root = Path(__file__).resolve().parents[3]
    sys.path.insert(0, str(repo_root / "localizer" / "lib"))
    return importlib.import_module("biomarker_region_viewer").BiomarkerRegionViewer


BiomarkerRegionViewer = _load_viewer_type()


def test_standard_biomarker_region_assets():
    viewer = BiomarkerRegionViewer("/tmp/plugin")

    assert viewer.asset_relative_path("voi", "Centiloid") == "voi_ctx_2mm.nii"
    assert viewer.asset_relative_path("ref", "Centiloid") == "voi_WhlCbl_2mm.nii"
    assert viewer.asset_relative_path("voi", "CenTauRz") == "CenTauR.nii"
    assert viewer.asset_relative_path("ref", "CenTauR") == "voi_CerebGry_tau_2mm.nii"
    assert viewer.asset_relative_path("mni") == "padded_MNI152_T1_2mm.nii"


def test_fill_states_voi_tracks_tracer_and_has_no_reference():
    viewer = BiomarkerRegionViewer("/tmp/plugin")

    assert viewer.asset_relative_path("voi", "Fill States", "FDG") == (
        "fill_states/fs_FDG_meta_roi.nii"
    )
    assert viewer.asset_relative_path("voi", "Fill States", "FTP") == "CenTauR.nii"
    assert viewer.asset_relative_path("ref", "Fill States", "FDG") is None


def test_asset_resolution_supports_source_checkout(tmp_path):
    asset = tmp_path / "src/assets/nii/voi_ctx_2mm.nii"
    asset.parent.mkdir(parents=True)
    asset.touch()
    viewer = BiomarkerRegionViewer(tmp_path)

    assert viewer.resolve_asset_path("voi_ctx_2mm.nii") == asset
