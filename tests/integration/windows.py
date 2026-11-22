"""Windows desktop, Unicode paths and the RAW-to-export workflow."""

import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile


def run(binary, *args, catalog):
    result = subprocess.run(
        [binary, *map(str, args), "--catalog", str(catalog)],
        encoding="utf-8",
        errors="replace",
        capture_output=True,
        timeout=30,
    )
    if result.returncode:
        raise AssertionError(
            f"{binary.name} {args[0]} exited {result.returncode}\n"
            f"{result.stdout}\n{result.stderr}"
        )
    return result.stdout


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def tiff_field(path, field):
    data = path.read_bytes()
    order = "<" if data.startswith(b"II") else ">"
    version, offset = struct.unpack_from(order + "HI", data, 2)
    assert version == 42, "Expected baseline TIFF"
    count = struct.unpack_from(order + "H", data, offset)[0]
    for index in range(count):
        start = offset + 2 + index * 12
        tag, kind, length = struct.unpack_from(order + "HHI", data, start)
        if tag == field:
            assert kind == 3 and 1 <= length <= 4
            value = start + 8 if length <= 2 else struct.unpack_from(order + "I", data, start + 8)[0]
            return struct.unpack_from(order + "H" * length, data, value)
    raise AssertionError(f"TIFF has no tag {field}")


def verify(app, cli, folder):
    assets = Path(__file__).resolve().parents[1] / "assets"
    incoming = folder / "Исходники"
    incoming.mkdir()
    output = folder / "Готовые"
    output.mkdir()
    catalog = folder / "Каталог.sqlite"
    raw = incoming / "Кадр.DNG"
    webp = incoming / "Прозрачность.webp"
    shutil.copyfile(assets / "migration-051" / "01-complete.DNG", raw)
    shutil.copyfile(assets / "images" / "quadrants.webp", webp)
    before = digest(raw), digest(webp)

    imports = json.loads(run(cli, "import", "copy", output, incoming, catalog=catalog))
    assert imports["added"] == 2 and imports["failed"] == 0, imports
    duplicates = json.loads(run(cli, "import", "add", incoming, catalog=catalog))
    assert duplicates["duplicates"] == 2, duplicates
    rows = json.loads(run(cli, "library", catalog=catalog))
    assert rows["total"] == 2, rows
    assert {item["filename"] for item in rows["items"]} == {raw.name, webp.name}
    assert {digest(Path(item["path"])) for item in rows["items"]} == set(before)

    edited = folder / "Проявка.sqlite"
    opened = json.loads(run(cli, "open", raw, catalog=edited))
    assert (opened["width"], opened["height"]) == (128, 96), opened
    run(cli, "exposure", "0.65", catalog=edited)
    restored = json.loads(run(cli, "restore", catalog=edited))
    assert restored["asset"] == opened["asset"] and restored["exposure"] == 0.65
    png, jpeg, tiff, dng = (output / f"Готово.{extension}" for extension in ("png", "jpg", "tif", "dng"))
    for destination in (png, jpeg, tiff, dng):
        run(cli, "export", destination, catalog=edited)
    hdr = output / "Яркие света.jpg"
    run(cli, "exposure", "3", catalog=edited)
    run(cli, "export", hdr, "--format", "jpeg-hdr", catalog=edited)
    assert b"urn:iso:std:iso:ts:21496:-1" in hdr.read_bytes()
    run(cli, "exposure", "0.65", catalog=edited)
    disconnected = raw.with_suffix(".offline")
    raw.rename(disconnected)
    try:
        offline = json.loads(run(cli, "restore", catalog=edited))
        assert offline["offlinePreview"] and offline["exposure"] == 0.65
        run(cli, "exposure", "0.9", catalog=edited)
        still_offline = json.loads(run(cli, "restore", catalog=edited))
        assert still_offline["offlinePreview"] and still_offline["exposure"] == 0.9
        run(app, "--test-restore", "0.9", catalog=edited)
        blocked = output / "Офлайн.png"
        result = subprocess.run([cli, "export", blocked, "--catalog", edited],
                                capture_output=True, timeout=30)
        assert result.returncode != 0 and not blocked.exists(), result
    finally:
        disconnected.rename(raw)
    connected = json.loads(run(cli, "restore", catalog=edited))
    assert not connected["offlinePreview"] and connected["exposure"] == 0.9
    run(cli, "exposure", "0.65", catalog=edited)
    reconnected = output / "Повтор.png"
    run(cli, "export", reconnected, catalog=edited)
    assert png.read_bytes() == reconnected.read_bytes(), "Offline editing changed export output"
    assert png.read_bytes().startswith(b"\x89PNG\r\n\x1a\n")
    assert jpeg.read_bytes().startswith(b"\xff\xd8")
    assert set(tiff_field(tiff, 258)) == {16}
    reopened = json.loads(run(cli, "open", dng, catalog=edited))
    assert (reopened["width"], reopened["height"]) == (96, 128), reopened
    assert before == (digest(raw), digest(webp)), "Original photo changed"
    run(cli, "catalog", "verify", catalog=edited)
    raster_catalog = folder / "Растр.sqlite"
    raster = json.loads(run(cli, "open", webp, catalog=raster_catalog))
    raster_output = output / "Альфа.png"
    run(cli, "export", raster_output, catalog=raster_catalog)
    assert raster_output.read_bytes()[25] == 6, "PNG lost its alpha channel"
    for depth in (8, 16):
        compressed = output / f"Сжатый {depth}.tif"
        run(cli, "export", compressed, "--format", f"tiff{depth}", "--compression", "zip",
            catalog=raster_catalog)
        assert tiff_field(compressed, 258) == (depth,) * 4
        assert tiff_field(compressed, 259) in {(8,), (32946,)}
        assert tiff_field(compressed, 338) == (2,), "TIFF lost its unassociated alpha"
        assert b"acsp" in compressed.read_bytes(), "TIFF lost its ICC profile"
        round_trip = folder / f"ZIP {depth}.sqlite"
        decoded = json.loads(run(cli, "open", compressed, catalog=round_trip))
        assert (decoded["width"], decoded["height"]) == (raster["width"], raster["height"]), decoded

    run(app, "--smoke-test", catalog=folder / "Интерфейс.sqlite")
    run(app, "--test-library", incoming, catalog=folder / "Обзор.sqlite")
    run(app, "--test-edit", raw, "--test-exposure", "0.65", "--test-develop",
        catalog=folder / "Редактор.sqlite")
    sidecar = folder / "Обработка.xmp"
    sidecar.write_text(
        '<x:xmpmeta xmlns:x="adobe:ns:meta/">'
        '<rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">'
        '<rdf:Description xmlns:crs="http://ns.adobe.com/camera-raw-settings/1.0/" '
        'crs:Exposure2012="1.3" crs:Texture="30" crs:CameraProfile="Example profile"/>'
        '</rdf:RDF></x:xmpmeta>',
        encoding="utf-8",
    )
    xmp_catalog = folder / "Правки XMP.sqlite"
    run(app, "--test-edit", raw, "--test-exposure", "1.3", "--test-xmp-preset", sidecar,
        catalog=xmp_catalog)
    assert json.loads(run(cli, "restore", catalog=xmp_catalog))["exposure"] == 1.3


if __name__ == "__main__":
    app, cli = (Path(path).resolve(strict=True) for path in sys.argv[1:])
    # The Windows sandbox's user temp path is long enough to overflow legacy
    # Win32 limits after the thumbnail cache's digest filenames are appended.
    with tempfile.TemporaryDirectory(prefix="apertone-windows-", dir=Path.cwd()) as temp:
        verify(app, cli, Path(temp))
