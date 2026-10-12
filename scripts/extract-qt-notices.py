"""Preserve readable license and attribution files from the shipped Qt sources."""
from pathlib import Path, PurePosixPath
import sys
import tarfile

archive, destination = Path(sys.argv[1]), Path(sys.argv[2])
with tarfile.open(archive, "r:xz") as source:
    for member in source:
        if not member.isfile():
            continue
        path = PurePosixPath(member.name)
        if path.is_absolute() or ".." in path.parts or len(path.parts) < 2:
            raise ValueError(f"Unsafe archive member: {member.name}")
        filename = path.name.lower()
        if ("LICENSES" not in path.parts
                and filename != "qt_attribution.json"
                and not any(word in filename for word in ("license", "copying", "copyright"))):
            continue
        target = destination.joinpath(*path.parts[1:])
        target.parent.mkdir(parents=True, exist_ok=True)
        with source.extractfile(member) as content:
            target.write_bytes(content.read())
