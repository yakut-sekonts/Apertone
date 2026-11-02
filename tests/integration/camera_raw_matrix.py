"""Validate vendor-independent Bayer routing with project-owned DNG sensor fixtures.

The generated TIFF/DNG payloads exercise Apertone's extension routing, LibRaw Bayer
contract, metadata, recursive import and XMP ownership. They do not claim coverage
of every proprietary compression variant used by real camera bodies.
"""

import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "assets"))
from generate_dng import create_dng


RAW_EXTENSIONS = (
    ".3fr", ".arw", ".cr2", ".cr3", ".crw", ".dcr", ".dcs", ".dng", ".erf",
    ".fff", ".gpr", ".iiq", ".k25", ".kdc", ".mef", ".mos", ".mrw", ".nef",
    ".nrw", ".orf", ".pef", ".ptx", ".raf", ".raw", ".rw2", ".rwl", ".sr2",
    ".srf", ".srw",
)

CAMERAS = (
    ("Canon", "EOS R5", ".cr3"),
    ("Canon", "EOS 5D Mark II", ".cr2"),
    ("Sony", "ILCE-7M4", ".arw"),
    ("Nikon", "Z 8", ".nef"),
    ("Nikon", "COOLPIX P7000", ".nrw"),
    ("Pentax", "K-1 Mark II", ".pef"),
    ("OM Digital Solutions", "OM-1", ".orf"),
    ("Panasonic", "DC-S5M2", ".rw2"),
    ("Leica Camera AG", "Q3", ".rwl"),
    ("Samsung", "NX1", ".srw"),
    ("Hasselblad", "X2D 100C", ".3fr"),
    ("Phase One", "IQ4", ".iiq"),
    ("Fujifilm", "X-A7 Bayer", ".raf"),
)


def run(cli: Path, catalog: Path, *arguments: object) -> dict:
    result = subprocess.run(
        [str(cli), *map(str, arguments), "--catalog", str(catalog)],
        capture_output=True,
        text=True,
        timeout=90,
    )
    if result.returncode != 0:
        raise AssertionError(
            f"Command failed ({result.returncode}): {arguments}\n{result.stdout}\n{result.stderr}"
        )
    return json.loads(result.stdout)


def verify_filters(source_root: Path) -> None:
    filters = "\n".join(
        (source_root / path).read_text()
        for path in ("apps/desktop/qml/Main.qml", "apps/desktop/qml/ImportDialog.qml")
    )
    for extension in RAW_EXTENSIONS:
        assert f"*{extension}" in filters and f"*{extension.upper()}" in filters, extension


def main() -> int:
    cli = Path(sys.argv[1]).resolve()
    source_root = Path(__file__).resolve().parents[2]
    verify_filters(source_root)
    with tempfile.TemporaryDirectory(prefix="apertone-camera-raw-") as temporary:
        root = Path(temporary)
        sources = root / "sources"
        sources.mkdir()

        for index, extension in enumerate(RAW_EXTENSIONS):
            create_dng(
                sources / f"registry-{index:02d}{extension.upper()}",
                maker="Apertone fixture",
                model=f"Registry {extension[1:].upper()}",
                shutter=(1, 100 + index),
            )

        for index, (maker, model, extension) in enumerate(CAMERAS):
            source = root / f"camera-{index:02d}{extension.upper()}"
            create_dng(source, maker=maker, model=model, shutter=(1, 200 + index))
            metadata = run(cli, root / f"open-{index}.sqlite", "open", source)
            assert metadata["width"] == 128 and metadata["height"] == 96
            assert model.upper() in metadata["camera"].upper(), metadata["camera"]

        dual = root / "dual-illuminant.DNG"
        create_dng(dual, maker="Apertone fixture", model="Dual Illuminant",
                   dual_illuminant=True)
        original_digest = hashlib.sha256(dual.read_bytes()).hexdigest()
        dual_catalog = root / "dual.sqlite"
        run(cli, dual_catalog, "open", dual)
        rendered = []
        for temperature in (2856, 6504):
            run(cli, dual_catalog, "develop", f"temperature={temperature}", "tint=0")
            destination = root / f"dual-{temperature}.png"
            run(cli, dual_catalog, "export", destination)
            rendered.append(hashlib.sha256(destination.read_bytes()).hexdigest())
        assert rendered[0] != rendered[1], (
            "Dual-illuminant DNG exports ignored the WB-dependent embedded profile"
        )
        assert hashlib.sha256(dual.read_bytes()).hexdigest() == original_digest

        summary = run(cli, root / "import.sqlite", "import", "add", sources)
        assert summary == {
            "session": summary["session"],
            "added": len(RAW_EXTENSIONS),
            "duplicates": 0,
            "failed": 0,
            "cancelled": False,
        }, summary
        library = run(cli, root / "import.sqlite", "library", "--limit", 64)
        assert library["total"] == len(RAW_EXTENSIONS)
        assert {Path(item["filename"]).suffix.lower() for item in library["items"]} == set(
            RAW_EXTENSIONS
        )

    print("Common Bayer RAW registry, vendor metadata, recursive import and UI filters passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
