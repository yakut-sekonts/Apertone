"""Mixed RAW/raster import, immutable originals, native UI, queue and restart."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "assets"))
from generate_dng import create_dng
sys.path.insert(0, str(Path(__file__).resolve().parent))
from develop import png_info

cli, app, fixtures = map(Path, sys.argv[1:4])
assets = Path(__file__).resolve().parents[1] / "assets/images"
env = {**os.environ, "QT_QPA_PLATFORM": "offscreen", "QT_QUICK_BACKEND": "software"}

def call(executable, *args, expected=0):
    result = subprocess.run([str(executable), *map(str, args)], capture_output=True,
                            text=True, env=env, timeout=100)
    assert result.returncode == expected, result.stdout + result.stderr
    assert not any(token in result.stderr for token in
                   ("ReferenceError", "Unable to assign", "Binding loop detected")), result.stderr
    return result.stdout

with tempfile.TemporaryDirectory(prefix="luma-images-") as temporary:
    root = Path(temporary)
    generated = root / "generated"
    call(fixtures, "--fixtures", generated)
    sources = root / "mixed"; sources.mkdir()
    for name in ("rgba.png", "opaque.jpg", "opaque.bmp", "ramp16.tif", "orientation-6.tif"):
        shutil.copyfile(generated / name, sources / name)
    for name in ("quadrants.webp", "two-frames.gif", "opaque.heic", "opaque.avif"):
        shutil.copyfile(assets / name, sources / name)
    # A camera RAW TIFF container must use full sensor decode, never a preview.
    create_dng(sources / "Camera.DNG", width=512, height=384)
    shutil.copyfile(sources / "Camera.DNG", root / "Camera.tiff")
    (sources / "notes.txt").write_text("Not an image")
    hashes = {p: hashlib.sha256(p.read_bytes()).hexdigest()
              for p in sources.iterdir() if p.suffix != ".txt"}
    catalog = root / "library.sqlite"
    def run(*args, db=catalog, expected=0):
        result = call(cli, *args, "--catalog", db, expected=expected)
        return json.loads(result) if not expected else result
    imported = run("import", "add", sources)
    # AVIF is optional on macOS versions whose native decoder does not list it.
    count = imported["added"]
    assert count in (9, 10), imported
    assert imported["failed"] == 0, imported
    assert run("import", "add", sources)["duplicates"] == count
    assert run("library")["total"] == count
    renamed = run("open", root / "Camera.tiff")
    assert (renamed["width"], renamed["height"]) == (512, 384)
    for source in hashes:
        if source.suffix == ".avif" and count == 9:
            continue
        opened = run("open", source)
        assert opened["width"] > 0 and opened["height"] > 0
        run("develop", "exposure=0.37", "contrast=12", "sharpen.amount=25", "noise.color=15")
        output = root / (source.name + ".png")
        run("export", output)
        expected_size = (48, 64, 8) if source.name == "orientation-6.tif" else (opened["width"], opened["height"], 8)
        assert png_info(output) == expected_size
        assert run("restore")["exposure"] == .37
        repeat = root / (source.name + "-restart.png")
        run("export", repeat)
        assert repeat.read_bytes() == output.read_bytes(), source
        run("export", source, "--overwrite", expected=1)
    copied = root / "copy"; copied.mkdir()
    copied_catalog = root / "copy.sqlite"
    assert run("import", "copy", copied, sources, db=copied_catalog)["added"] == count
    with sqlite3.connect(copied_catalog) as db:
        for path, checksum in db.execute("SELECT original_path,checksum FROM assets"):
            assert Path(path).is_relative_to(copied.resolve())
            assert hashlib.sha256(Path(path).read_bytes()).hexdigest() == checksum
    # Existing native Library acceptance expects exactly two assets.
    pair = root / "pair"; pair.mkdir()
    for name in ("rgba.png", "opaque.jpg"):
        shutil.copyfile(sources / name, pair / name)
    gui_catalog = root / "gui.sqlite"
    assert "Library GUI acceptance passed" in call(app, "--catalog", gui_catalog, "--test-library", pair)
    thumbs = list((root / "gui.sqlite.luma-cache/thumbnails").glob("*.jpg"))
    assert len(thumbs) >= 3 and all(p.name.endswith("-edited-v2.jpg") for p in thumbs)
    first, second = root / "first.png", root / "second.png"
    call(app, "--catalog", gui_catalog, "--test-edit", sources / "rgba.png", "--test-exposure", ".65", "--test-export", first)
    call(app, "--catalog", gui_catalog, "--test-restore", ".65", "--test-export", second)
    assert first.read_bytes() == second.read_bytes()
    queue = root / "queue"; queue.mkdir()
    call(app, "--catalog", gui_catalog, "--test-edit", sources / "rgba.png", "--test-export-queue", queue)
    dng = next(queue.glob("*.dng"))
    reopened = run("open", dng, db=root / "reimport.sqlite")
    assert (reopened["width"], reopened["height"]) == (512, 384)
    # Rendered DNG contains a smaller preview; TIFF renaming must not select it.
    renamed_dng = root / "rendered.tif"; shutil.copyfile(dng, renamed_dng)
    reopened = run("open", renamed_dng, db=root / "reimport-tif.sqlite")
    assert (reopened["width"], reopened["height"]) == (512, 384)
    for path, checksum in hashes.items():
        assert hashlib.sha256(path.read_bytes()).hexdigest() == checksum
    assert run("catalog", "verify")["integrity"] == "ok"
    print("Mixed RAW/JPEG/PNG/TIFF/HEIC/WebP/BMP/GIF/optional AVIF: Add/Copy/duplicates, edit/export/restart, native Library/thumbnails/queue, RAW TIFF routing and original hashes passed")
