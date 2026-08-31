import json

import nibabel as nib
import numpy as np


class TestBrainParcellateCLI:
    def test_basic(self, run_subprocess, tmp_path, test_files):
        output_dir = tmp_path / "parcellation"
        result = run_subprocess(
            [
                "brain-parcellate",
                "--input",
                str(test_files["input"]),
                "--output",
                str(output_dir),
            ]
        )

        assert result.returncode == 0, (
            "brain-parcellate command failed.\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )

        expected_masks = {
            "ventricles",
            "left_hippocampus",
            "right_hippocampus",
            "whole_brain",
            "left_entorhinal",
            "right_entorhinal",
            "left_fusiform",
            "right_fusiform",
            "left_middle_temporal",
            "right_middle_temporal",
            "intracranial",
        }
        source = nib.load(test_files["input"])
        for name in expected_masks:
            mask = nib.load(output_dir / f"{name}_mask.nii.gz")
            values = np.asarray(mask.dataobj)
            assert mask.shape == source.shape
            assert np.allclose(mask.affine, source.affine)
            assert mask.get_data_dtype() == np.dtype("uint8")
            assert set(np.unique(values)).issubset({0, 1})
            assert values.any()

        summary = json.loads((output_dir / "volumes.json").read_text())
        assert summary["units"] == "mL"
        bilateral = summary["Hippocampus"]["bilateral_volume_ml"]
        left_plus_right = (
            summary["Hippocampus"]["left_volume_ml"]
            + summary["Hippocampus"]["right_volume_ml"]
        )
        assert abs(bilateral - left_plus_right) < 1e-5
        assert "not FreeSurfer eTIV" in summary["ICV"]["definition"]

    def test_rejects_batch_mode(self, run_subprocess, tmp_path, test_files):
        result = run_subprocess(
            [
                "brain-parcellate",
                "--input",
                str(test_files["input"]),
                "--output",
                str(tmp_path / "output"),
                "--batch",
            ]
        )

        assert result.returncode != 0
        assert "not supported" in result.stderr
