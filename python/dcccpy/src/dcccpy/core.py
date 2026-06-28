from __future__ import annotations

import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Mapping, Sequence


class DCCCcoreNotFoundError(FileNotFoundError):
    """Raised when dcccpy cannot locate a DCCCcore executable."""


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


def _platform_key() -> str:
    machine = platform.machine().lower()
    if machine in {"x86_64", "amd64"}:
        arch = "x86_64"
    elif machine in {"aarch64", "arm64"}:
        arch = "arm64"
    else:
        arch = machine

    if sys.platform.startswith("linux"):
        return f"linux-{arch}"
    if sys.platform == "darwin":
        return f"macos-{arch}"
    if sys.platform.startswith("win"):
        return f"windows-{arch}"
    return f"{sys.platform}-{arch}"


def _candidate_names() -> tuple[str, ...]:
    return ("DCCCcore.exe", "DCCCcore") if os.name == "nt" else ("DCCCcore",)


def _package_root() -> Path:
    return Path(__file__).resolve().parent


def _vendored_candidates() -> list[Path]:
    vendor_root = _package_root() / "vendor" / "dccccore"
    platform_dir = vendor_root / _platform_key()
    candidates: list[Path] = []

    for base in (platform_dir, vendor_root):
        for name in _candidate_names():
            candidates.append(base / name)
    return candidates


def dccccore_path(executable: str | os.PathLike[str] | None = None) -> Path:
    """Return the native DCCCcore executable path."""

    explicit = executable or os.environ.get("DCCCPY_DCCCCORE")
    if explicit:
        path = Path(explicit).expanduser()
        if path.exists():
            return path
        raise DCCCcoreNotFoundError(f"DCCCcore executable does not exist: {path}")

    for candidate in _vendored_candidates():
        if candidate.exists():
            return candidate

    found = shutil.which("DCCCcore")
    if found:
        return Path(found)

    raise DCCCcoreNotFoundError(
        "Could not find DCCCcore. Install a dcccpy wheel with vendored DCCCcore, "
        "set DCCCPY_DCCCCORE, or put DCCCcore on PATH."
    )


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


def _append_flag(args: list[str], flag: str, enabled: bool) -> None:
    if enabled:
        args.append(flag)


def _metric_command(
    subcommand: str,
    input: str | os.PathLike[str],
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
    temp_dir: Path | None = None
    actual_output: Path

    if output is None:
        actual_output, temp_dir = _temp_output()
    else:
        actual_output = Path(output)

    args = [subcommand, "--input", os.fspath(input), "--output", os.fspath(actual_output)]
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
    input: str | os.PathLike[str],
    output: str | os.PathLike[str] | None = None,
    *,
    suvr: bool = False,
    **kwargs: object,
) -> DCCCResult:
    """Run the DCCCcore centiloid command."""

    return _metric_command("centiloid", input, output, **_coerce_metric_kwargs(kwargs, suvr=suvr))


def centaur(
    input: str | os.PathLike[str],
    output: str | os.PathLike[str] | None = None,
    *,
    suvr: bool = False,
    **kwargs: object,
) -> DCCCResult:
    return _metric_command("centaur", input, output, **_coerce_metric_kwargs(kwargs, suvr=suvr))


def centaurz(
    input: str | os.PathLike[str],
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
    input: str | os.PathLike[str],
    output: str | os.PathLike[str] | None = None,
    **kwargs: object,
) -> DCCCResult:
    return _metric_command("abetaindex", input, output, **_coerce_metric_kwargs(kwargs))


def abetaload(
    input: str | os.PathLike[str],
    output: str | os.PathLike[str] | None = None,
    **kwargs: object,
) -> DCCCResult:
    return _metric_command("abetaload", input, output, **_coerce_metric_kwargs(kwargs))


def fillstates(
    input: str | os.PathLike[str],
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
    input: str | os.PathLike[str],
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
    input: str | os.PathLike[str],
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
    input: str | os.PathLike[str],
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
    temp_dir: Path | None = None
    if output is None:
        actual_output, temp_dir = _temp_output()
    else:
        actual_output = Path(output)

    args = [subcommand, "--input", os.fspath(input), "--output", os.fspath(actual_output)]
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
    input: str | os.PathLike[str],
    output: str | os.PathLike[str] | None = None,
    **kwargs: object,
) -> DCCCResult:
    return _spatial_command("normalize", input, output, **kwargs)


def adni_pet_core(
    input: str | os.PathLike[str],
    output: str | os.PathLike[str] | None = None,
    **kwargs: object,
) -> DCCCResult:
    return _spatial_command("adni-pet-core", input, output, **kwargs)


def rigid(
    input: str | os.PathLike[str],
    output: str | os.PathLike[str] | None = None,
    **kwargs: object,
) -> DCCCResult:
    return _spatial_command("rigid", input, output, **kwargs)
