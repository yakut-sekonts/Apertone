"""Exercise shared CLI engine and real Qt controller across separate processes."""
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "assets"))
from generate_dng import create_dng


def main():
    cli, mode = Path(sys.argv[1]), sys.argv[2]
    gui = cli.parent.parent / "Luma.app/Contents/MacOS/Luma"
    with tempfile.TemporaryDirectory(prefix="luma-acceptance-") as temporary:
        directory = Path(temporary)
        catalog = directory / "catalog.sqlite"
        env = os.environ.copy()
        env["QT_QPA_PLATFORM"] = "offscreen"
        env["QT_QUICK_BACKEND"] = "software"

        def run(*arguments, expected=0):
            result = subprocess.run([str(cli), *map(str, arguments), "--catalog", str(catalog)], capture_output=True, text=True, timeout=90)
            if result.returncode != expected:
                raise AssertionError(f"Command failed: {arguments}: {result.stdout} {result.stderr}")
            return json.loads(result.stdout) if expected == 0 else result

        if mode == "synthetic":
            source = directory / "Кадр с пробелами.DNG"
            create_dng(source)
            initial = run("open", source)
        else:
            root = os.environ.get("LUMA_PRIVATE_TEST_ASSETS") or os.environ.get("PHOTON_PRIVATE_TEST_ASSETS")
            if not root or not Path(root).is_dir():
                print("SKIP: set LUMA_PRIVATE_TEST_ASSETS (or PHOTON_PRIVATE_TEST_ASSETS) to a directory containing a Nikon D7500 NEF.")
                return 77
            source = None
            for candidate in sorted(Path(root).rglob("*")):
                if candidate.suffix.lower() != ".nef":
                    continue
                metadata = run("open", candidate)
                if "D7500" in metadata["camera"].upper():
                    source, initial = candidate, metadata
                    break
            if source is None:
                print("SKIP: no Nikon D7500 NEF found in the private directory.")
                return 77
            for field in ["iso", "shutter", "aperture", "focalLength"]:
                assert initial[field] is not None and initial[field] > 0, field

        before = hashlib.sha256(source.read_bytes()).hexdigest()
        assert initial["width"] > 0 and initial["height"] > 0
        run("exposure", 1.25)
        restored = run("restore")
        assert restored["asset"] == initial["asset"] and restored["exposure"] == 1.25
        run("exposure", "nan", expected=1)
        assert run("restore")["exposure"] == 1.25

        for arguments in [("--test-edit", str(source), "--test-exposure", "-0.75"), ("--test-restore", "-0.75")]:
            result = subprocess.run([str(gui), "--catalog", str(catalog), *arguments], env=env, capture_output=True, text=True, timeout=100)
            assert result.returncode == 0, f"exit={result.returncode}: {result.stdout} {result.stderr}"
            assert "RAW viewer and Exposure persistence verified" in result.stdout
        assert run("restore")["exposure"] == -0.75
        assert before == hashlib.sha256(source.read_bytes()).hexdigest(), "Original was modified"
        assert run("catalog", "verify")["integrity"] == "ok"
        with sqlite3.connect(catalog) as connection:
            assert connection.execute("PRAGMA journal_mode").fetchone()[0] == "wal"
            assert connection.execute("SELECT COUNT(*) FROM assets").fetchone()[0] == 1
        if mode == "synthetic":
            missing = source.with_suffix(".temporarily-missing")
            source.rename(missing)
            run("restore", expected=1)
            with sqlite3.connect(catalog) as connection:
                assert connection.execute("SELECT exposure FROM edit_documents").fetchone()[0] == -0.75
            missing.rename(source)
            assert run("restore")["exposure"] == -0.75
        print(f"{mode}: true RAW decode, EXIF, GUI viewer, autosave, restart and original integrity passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
