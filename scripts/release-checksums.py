#!/usr/bin/env python3
"""Write checksums for the assembled Release ZIP files."""
import hashlib
from pathlib import Path
import sys

root = Path(sys.argv[1])
lines = []
for path in sorted(root.glob("*.zip")):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    lines.append(f"{digest.hexdigest()}  {path.name}\n")
if not lines:
    raise SystemExit("No release ZIP files to checksum")
(root / "SHA256SUMS").write_text("".join(lines))
