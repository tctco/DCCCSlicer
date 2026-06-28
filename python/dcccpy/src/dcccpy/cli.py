from __future__ import annotations

import sys

from .core import run
from .runtime import DCCCcoreDownloadError, DCCCcoreNotFoundError


def main(argv: list[str] | None = None) -> int:
    args = list(sys.argv[1:] if argv is None else argv)
    try:
        result = run(args, check=False)
    except (DCCCcoreDownloadError, DCCCcoreNotFoundError) as exc:
        print(str(exc), file=sys.stderr)
        return 127

    if result.stdout:
        print(result.stdout, end="")
    if result.stderr:
        print(result.stderr, file=sys.stderr, end="")
    return result.returncode
