import gzip
import struct

import numpy as np


_NIFTI_DTYPES = {
    2: np.uint8,
    4: np.int16,
    8: np.int32,
    16: np.float32,
    64: np.float64,
    512: np.uint16,
}


def _open_nifti(path, mode):
    return gzip.open(path, mode) if str(path).endswith(".gz") else open(path, mode)


def _write_float_nifti(path, data):
    """Write a small identity-geometry NIfTI without adding a test-only imaging dependency."""
    data = np.asarray(data, dtype="<f4")
    if data.ndim not in (3, 4):
        raise ValueError("Test NIfTI data must be 3D or 4D")

    dimensions = list(reversed(data.shape))
    header = bytearray(348)
    struct.pack_into("<i", header, 0, 348)
    struct.pack_into("<8h", header, 40, data.ndim, *dimensions, *([1] * (7 - data.ndim)))
    struct.pack_into("<h", header, 70, 16)
    struct.pack_into("<h", header, 72, 32)
    struct.pack_into("<8f", header, 76, 1.0, *([1.0] * 7))
    struct.pack_into("<f", header, 108, 352.0)
    struct.pack_into("<f", header, 112, 1.0)
    struct.pack_into("<B", header, 123, 2)
    struct.pack_into("<h", header, 254, 1)
    struct.pack_into("<4f", header, 280, 1.0, 0.0, 0.0, 0.0)
    struct.pack_into("<4f", header, 296, 0.0, 1.0, 0.0, 0.0)
    struct.pack_into("<4f", header, 312, 0.0, 0.0, 1.0, 0.0)
    header[344:348] = b"n+1\x00"
    with _open_nifti(path, "wb") as output:
        output.write(header)
        output.write(b"\x00\x00\x00\x00")
        output.write(data.tobytes(order="C"))


def _read_nifti(path):
    with _open_nifti(path, "rb") as source:
        payload = source.read()
    endian = "<" if struct.unpack_from("<i", payload, 0)[0] == 348 else ">"
    ndim = struct.unpack_from(f"{endian}h", payload, 40)[0]
    dimensions = struct.unpack_from(f"{endian}{ndim}h", payload, 42)
    datatype = struct.unpack_from(f"{endian}h", payload, 70)[0]
    offset = int(struct.unpack_from(f"{endian}f", payload, 108)[0])
    dtype = np.dtype(_NIFTI_DTYPES[datatype]).newbyteorder(endian)
    count = int(np.prod(dimensions))
    data = np.frombuffer(payload, dtype=dtype, count=count, offset=offset).copy()
    slope = struct.unpack_from(f"{endian}f", payload, 112)[0]
    intercept = struct.unpack_from(f"{endian}f", payload, 116)[0]
    if slope != 0.0:
        data = data.astype(np.float32) * slope + intercept
    return data.reshape(tuple(reversed(dimensions))), ndim


def _nifti_datatype(path):
    with _open_nifti(path, "rb") as source:
        header = source.read(74)
    endian = "<" if struct.unpack_from("<i", header, 0)[0] == 348 else ">"
    return struct.unpack_from(f"{endian}h", header, 70)[0]


def _synthetic_pet(shape=(32, 32, 32)):
    z, y, x = np.indices(shape, dtype=np.float32)
    return (
        1.1 * np.exp(-((x - 10.0) ** 2 / 20.0 + (y - 13.0) ** 2 / 35.0 + (z - 17.0) ** 2 / 26.0))
        + 0.8 * np.exp(-((x - 22.0) ** 2 / 12.0 + (y - 20.0) ** 2 / 20.0 + (z - 10.0) ** 2 / 18.0))
        + 0.45 * np.exp(-((x - 18.0) ** 2 / 8.0 + (y - 8.0) ** 2 / 9.0 + (z - 23.0) ** 2 / 12.0))
    ).astype(np.float32)


def _transformed_pet(angle_degrees, translation, shape=(32, 32, 32)):
    z, y, x = np.indices(shape, dtype=np.float32)
    center = (np.asarray(shape[::-1], dtype=np.float32) - 1.0) / 2.0
    points = np.stack((x, y, z), axis=-1)
    angle = np.deg2rad(angle_degrees)
    rotation = np.array(
        [[np.cos(angle), -np.sin(angle), 0.0],
         [np.sin(angle), np.cos(angle), 0.0],
         [0.0, 0.0, 1.0]],
        dtype=np.float32,
    )
    source = (points - center - np.asarray(translation, dtype=np.float32)) @ rotation + center
    sx, sy, sz = source[..., 0], source[..., 1], source[..., 2]
    return (
        1.1 * np.exp(-((sx - 10.0) ** 2 / 20.0 + (sy - 13.0) ** 2 / 35.0 + (sz - 17.0) ** 2 / 26.0))
        + 0.8 * np.exp(-((sx - 22.0) ** 2 / 12.0 + (sy - 20.0) ** 2 / 20.0 + (sz - 10.0) ** 2 / 18.0))
        + 0.45 * np.exp(-((sx - 18.0) ** 2 / 8.0 + (sy - 8.0) ** 2 / 9.0 + (sz - 23.0) ** 2 / 12.0))
    ).astype(np.float32)


class TestPetMotionCorrectCLI:
    def test_rejects_3d_input(self, run_subprocess, tmp_path):
        input_path = tmp_path / "single_pet.nii.gz"
        output_path = tmp_path / "average.nii.gz"
        _write_float_nifti(input_path, _synthetic_pet())

        result = run_subprocess([
            "pet-motion-correct", "--input", str(input_path), "--output", str(output_path)
        ])

        assert result.returncode != 0
        assert "multi-frame/4D PET" in result.stderr
        assert not output_path.exists()

    def test_single_frame_4d_is_preserved(self, run_subprocess, tmp_path):
        frame = _synthetic_pet((20, 21, 22))
        input_path = tmp_path / "one_frame.nii.gz"
        output_path = tmp_path / "average.nii.gz"
        corrected_path = tmp_path / "corrected.nii.gz"
        motion_path = tmp_path / "motion.tsv"
        _write_float_nifti(input_path, frame[np.newaxis, ...])

        result = run_subprocess([
            "pet-motion-correct",
            "--input", str(input_path),
            "--output", str(output_path),
            "--save-corrected-dynamic", str(corrected_path),
            "--motion-output", str(motion_path),
        ])

        assert result.returncode == 0, result.stderr
        averaged, averaged_dimension = _read_nifti(output_path)
        corrected, corrected_dimension = _read_nifti(corrected_path)
        assert averaged_dimension == 3
        assert _nifti_datatype(output_path) == 16
        assert corrected_dimension == 4
        np.testing.assert_allclose(averaged, frame, rtol=0.0, atol=1e-6)
        np.testing.assert_allclose(corrected[0], frame, rtol=0.0, atol=1e-6)
        assert motion_path.read_text().splitlines() == [
            "frame\ttx\tty\ttz\trx\try\trz",
            "0\t0\t0\t0\t0\t0\t0",
        ]

    def test_known_rigid_motion_improves_alignment(self, run_subprocess, tmp_path):
        fixed = _synthetic_pet()
        frames = np.stack([
            fixed,
            _transformed_pet(6.0, (2.0, -1.5, 1.0)),
            _transformed_pet(-5.0, (-1.5, 2.0, -1.0)),
        ])
        input_path = tmp_path / "moving_frames.nii.gz"
        output_path = tmp_path / "average.nii.gz"
        corrected_path = tmp_path / "corrected.nii.gz"
        motion_path = tmp_path / "motion.tsv"
        _write_float_nifti(input_path, frames)

        result = run_subprocess([
            "pet-motion-correct",
            "--input", str(input_path),
            "--output", str(output_path),
            "--save-corrected-dynamic", str(corrected_path),
            "--motion-output", str(motion_path),
        ])

        assert result.returncode == 0, result.stderr
        corrected, dimension = _read_nifti(corrected_path)
        averaged, averaged_dimension = _read_nifti(output_path)
        assert dimension == 4
        assert averaged_dimension == 3
        assert _nifti_datatype(output_path) == 16
        before_mse = np.mean((frames[1:] - fixed) ** 2)
        after_mse = np.mean((corrected[1:] - fixed) ** 2)
        assert after_mse < before_mse * 0.35
        np.testing.assert_allclose(averaged, corrected.mean(axis=0), rtol=1e-5, atol=1e-6)
        rows = motion_path.read_text().splitlines()
        assert len(rows) == 4
        assert rows[1] == "0\t0\t0\t0\t0\t0\t0"


class TestAdniPetCoreDynamicInput:
    def test_single_frame_4d_runs_automatic_preprocessing(
        self, run_subprocess, tmp_path, test_files
    ):
        original = test_files["input"].read_bytes()
        dynamic_header = bytearray(original)
        endian = "<" if struct.unpack_from("<i", dynamic_header, 0)[0] == 348 else ">"
        original_dimension = struct.unpack_from(f"{endian}h", dynamic_header, 40)[0]
        assert original_dimension == 3
        struct.pack_into(f"{endian}h", dynamic_header, 40, 4)
        struct.pack_into(f"{endian}h", dynamic_header, 48, 1)
        dynamic_path = tmp_path / "single_frame_dynamic.nii"
        dynamic_path.write_bytes(dynamic_header)
        output_path = tmp_path / "adni_dynamic_output.nii"

        result = run_subprocess([
            "adni-pet-core", "--input", str(dynamic_path), "--output", str(output_path)
        ])

        assert result.returncode == 0, (
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )
        assert "4D PET detected" in result.stdout
        _, output_dimension = _read_nifti(output_path)
        assert output_dimension == 3
