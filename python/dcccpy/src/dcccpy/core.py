from __future__ import annotations

import os
import re
import subprocess
import tempfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Mapping, Sequence

from .runtime import DCCCcoreNotFoundError, dccccore_path


_NUMBER_RE = r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?"
_METRIC_RE = re.compile(rf"^\s*([A-Za-z][A-Za-z0-9_. /()-]*):\s*({_NUMBER_RE})\s*(?:%.*)?$")


@dataclass(frozen=True)
class DCCCResult:
    """Completed DCCCcore invocation."""

    args: tuple[str, ...]
    returncode: int
    stdout: str
    stderr: str
    output: Path | None = None
    temp_dir: Path | None = None
    executable: Path | None = None
    metrics: Mapping[str, float] = field(default_factory=dict)

    @property
    def command(self) -> tuple[str, ...]:
        if self.executable is None:
            return self.args
        return (str(self.executable), *self.args)

    def check_returncode(self) -> "DCCCResult":
        if self.returncode:
            raise subprocess.CalledProcessError(
                self.returncode,
                list(self.command),
                output=self.stdout,
                stderr=self.stderr,
            )
        return self

    def load_output(self):
        """Load the output image with nibabel."""

        if self.output is None:
            raise ValueError("This DCCCResult does not have an output path.")
        try:
            import nibabel as nib
        except ImportError as exc:
            raise ImportError("Install dcccpy[nibabel] or nibabel to load output images.") from exc
        return nib.load(os.fspath(self.output))


def parse_metrics(text: str) -> dict[str, float]:
    """Parse numeric metric lines from DCCCcore stdout."""

    metrics: dict[str, float] = {}
    for line in text.splitlines():
        match = _METRIC_RE.match(line)
        if not match:
            continue
        label = " ".join(match.group(1).strip().split())
        metrics[label] = float(match.group(2))
    return metrics


def _as_args(args: Sequence[object]) -> list[str]:
    return [os.fspath(arg) if isinstance(arg, os.PathLike) else str(arg) for arg in args]


def run(
    args: Sequence[object],
    *,
    check: bool = True,
    executable: str | os.PathLike[str] | None = None,
    cwd: str | os.PathLike[str] | None = None,
    env: Mapping[str, str] | None = None,
    output: str | os.PathLike[str] | None = None,
) -> DCCCResult:
    """Run DCCCcore with raw command-line arguments."""

    exe = dccccore_path(executable)
    str_args = _as_args(args)
    completed = subprocess.run(
        [str(exe), *str_args],
        cwd=os.fspath(cwd) if cwd is not None else None,
        env={**os.environ, **dict(env or {})},
        text=True,
        capture_output=True,
        check=False,
    )
    result = DCCCResult(
        args=tuple(str_args),
        returncode=completed.returncode,
        stdout=completed.stdout,
        stderr=completed.stderr,
        output=Path(output) if output is not None else None,
        executable=exe,
        metrics=parse_metrics(completed.stdout),
    )
    return result.check_returncode() if check else result


def _temp_output(suffix: str = ".nii") -> tuple[Path, Path]:
    temp_dir = Path(tempfile.mkdtemp(prefix="dcccpy-"))
    return temp_dir / f"output{suffix}", temp_dir


def _create_temp_dir(existing: Path | None = None) -> Path:
    return existing or Path(tempfile.mkdtemp(prefix="dcccpy-"))


def _prepare_input(input: object, temp_dir: Path | None) -> tuple[str, Path | None]:
    if isinstance(input, (str, os.PathLike)):
        return os.fspath(input), temp_dir

    to_filename = getattr(input, "to_filename", None)
    if callable(to_filename):
        temp_dir = _create_temp_dir(temp_dir)
        input_path = temp_dir / "input.nii.gz"
        to_filename(os.fspath(input_path))
        return os.fspath(input_path), temp_dir

    raise TypeError(
        "input must be a filesystem path or a nibabel-like image object with to_filename()."
    )


def _prepare_io(input: object, output: str | os.PathLike[str] | None) -> tuple[str, Path, Path | None]:
    temp_dir: Path | None = None
    if output is None:
        actual_output, temp_dir = _temp_output()
    else:
        actual_output = Path(output)

    input_path, temp_dir = _prepare_input(input, temp_dir)
    return input_path, actual_output, temp_dir


def _append_flag(args: list[str], flag: str, enabled: bool) -> None:
    if enabled:
        args.append(flag)


def _metric_command(
    subcommand: str,
    input: object,
    output: str | os.PathLike[str] | None,
    *,
    batch: bool = False,
    bids: str | None = None,
    config: str | os.PathLike[str] | None = None,
    skip_normalization: bool = False,
    suvr_values: bool = False,
    debug: bool = False,
    extra_args: Sequence[object] = (),
    check: bool = True,
    executable: str | os.PathLike[str] | None = None,
) -> DCCCResult:
    input_path, actual_output, temp_dir = _prepare_io(input, output)

    args = [subcommand, "--input", input_path, "--output", os.fspath(actual_output)]
    _append_flag(args, "--batch", batch)
    _append_flag(args, "--skip-normalization", skip_normalization)
    _append_flag(args, "--suvr", suvr_values)
    _append_flag(args, "--debug", debug)
    if bids is not None:
        args.extend(["--bids", bids])
    if config is not None:
        args.extend(["--config", os.fspath(config)])
    args.extend(_as_args(extra_args))

    result = run(args, check=check, executable=executable, output=actual_output)
    return DCCCResult(
        args=result.args,
        returncode=result.returncode,
        stdout=result.stdout,
        stderr=result.stderr,
        output=actual_output,
        temp_dir=temp_dir,
        executable=result.executable,
        metrics=result.metrics,
    )


def _coerce_metric_kwargs(kwargs: dict[str, object], *, suvr: bool = False) -> dict[str, object]:
    coerced = dict(kwargs)
    if "include_suvr" in coerced and "suvr_values" not in coerced:
        coerced["suvr_values"] = coerced.pop("include_suvr")
    if "suvr" in coerced and "suvr_values" not in coerced:
        coerced["suvr_values"] = coerced.pop("suvr")
    if suvr and "suvr_values" not in coerced:
        coerced["suvr_values"] = True
    return coerced


def centiloid(
    input: object,
    output: str | os.PathLike[str] | None = None,
    *,
    suvr: bool = False,
    **kwargs: object,
) -> DCCCResult:
    """Run the DCCCcore centiloid command."""

    return _metric_command("centiloid", input, output, **_coerce_metric_kwargs(kwargs, suvr=suvr))


def centaur(
    input: object,
    output: str | os.PathLike[str] | None = None,
    *,
    suvr: bool = False,
    **kwargs: object,
) -> DCCCResult:
    return _metric_command("centaur", input, output, **_coerce_metric_kwargs(kwargs, suvr=suvr))


def centaurz(
    input: object,
    output: str | os.PathLike[str] | None = None,
    *,
    suvr: bool = False,
    report_detailed_regions: bool = False,
    **kwargs: object,
) -> DCCCResult:
    coerced = _coerce_metric_kwargs(kwargs, suvr=suvr)
    extra_args = list(coerced.pop("extra_args", ()))
    if report_detailed_regions:
        extra_args.append("--report-detailed-regions")
    return _metric_command("centaurz", input, output, extra_args=extra_args, **coerced)


def abetaindex(
    input: object,
    output: str | os.PathLike[str] | None = None,
    **kwargs: object,
) -> DCCCResult:
    return _metric_command("abetaindex", input, output, **_coerce_metric_kwargs(kwargs))


def abetaload(
    input: object,
    output: str | os.PathLike[str] | None = None,
    **kwargs: object,
) -> DCCCResult:
    return _metric_command("abetaload", input, output, **_coerce_metric_kwargs(kwargs))


def fillstates(
    input: object,
    output: str | os.PathLike[str] | None = None,
    *,
    tracer: str,
    suvr: bool = False,
    **kwargs: object,
) -> DCCCResult:
    coerced = _coerce_metric_kwargs(kwargs, suvr=suvr)
    extra_args = list(coerced.pop("extra_args", ()))
    extra_args.extend(["--tracer", tracer])
    return _metric_command("fillstates", input, output, extra_args=extra_args, **coerced)


def suvr(
    input: object,
    output: str | os.PathLike[str] | None = None,
    *,
    voi_mask: str | os.PathLike[str],
    ref_mask: str | os.PathLike[str],
    **kwargs: object,
) -> DCCCResult:
    coerced = _coerce_metric_kwargs(kwargs)
    extra_args = list(coerced.pop("extra_args", ()))
    extra_args.extend(["--voi-mask", os.fspath(voi_mask), "--ref-mask", os.fspath(ref_mask)])
    return _metric_command("suvr", input, output, extra_args=extra_args, **coerced)


def adad(
    input: object,
    output: str | os.PathLike[str] | None = None,
    *,
    modality: str | None = None,
    iterative: bool = False,
    **kwargs: object,
) -> DCCCResult:
    coerced = _coerce_metric_kwargs(kwargs)
    extra_args = list(coerced.pop("extra_args", ()))
    if modality is not None:
        extra_args.extend(["--modality", modality])
    if iterative:
        extra_args.append("--iterative")
    return _metric_command("adad", input, output, extra_args=extra_args, **coerced)


def _spatial_command(
    subcommand: str,
    input: object,
    output: str | os.PathLike[str] | None = None,
    *,
    iterative: bool = False,
    manual_fov: bool = False,
    batch: bool = False,
    bids: str | None = None,
    extra_args: Sequence[object] = (),
    check: bool = True,
    executable: str | os.PathLike[str] | None = None,
) -> DCCCResult:
    input_path, actual_output, temp_dir = _prepare_io(input, output)

    args = [subcommand, "--input", input_path, "--output", os.fspath(actual_output)]
    _append_flag(args, "--iterative", iterative)
    _append_flag(args, "--manual-fov", manual_fov)
    _append_flag(args, "--batch", batch)
    if bids is not None:
        args.extend(["--bids", bids])
    args.extend(_as_args(extra_args))

    result = run(args, check=check, executable=executable, output=actual_output)
    return DCCCResult(
        args=result.args,
        returncode=result.returncode,
        stdout=result.stdout,
        stderr=result.stderr,
        output=actual_output,
        temp_dir=temp_dir,
        executable=result.executable,
        metrics=result.metrics,
    )


def normalize(
    input: object,
    output: str | os.PathLike[str] | None = None,
    **kwargs: object,
) -> DCCCResult:
    return _spatial_command("normalize", input, output, **kwargs)


def adni_pet_core(
    input: object,
    output: str | os.PathLike[str] | None = None,
    **kwargs: object,
) -> DCCCResult:
    return _spatial_command("adni-pet-core", input, output, **kwargs)


def rigid(
    input: object,
    output: str | os.PathLike[str] | None = None,
    **kwargs: object,
) -> DCCCResult:
    return _spatial_command("rigid", input, output, **kwargs)
