"""Exercise the Windows filesystem driver on a test exFAT volume."""

from contextlib import closing
import ctypes
import hashlib
import json
from pathlib import Path
import sqlite3
import struct
import subprocess
import sys
import tempfile
import time


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def run(cli, catalog, *args):
    result = subprocess.run([cli, *map(str, args), "--catalog", catalog], capture_output=True, timeout=240)
    assert result.returncode == 0, result.stderr.decode("utf-8", "replace")
    return json.loads(result.stdout)


def verify(cli, volume, scratch):
    filesystem = ctypes.create_unicode_buffer(32)
    assert ctypes.windll.kernel32.GetVolumeInformationW(str(volume), None, 0, None, None, None, filesystem, 32)
    assert filesystem.value.casefold() == "exfat"
    fixture = Path(__file__).resolve().parents[1] / "assets/migration-051/01-complete.DNG"
    data = fixture.read_bytes()
    destination = scratch / "copied"
    destination.mkdir()
    catalog = scratch / "exfat-catalog.sqlite"
    with tempfile.TemporaryDirectory(prefix="Apertone-", dir=volume) as directory:
        source = Path(directory) / "DCIM" / "100TEST"
        source.mkdir(parents=True)
        expected = {}
        for index in range(1000):
            name = f"Кадр-{index:04}.DNG"
            path = source / name
            path.write_bytes(data + struct.pack("<I", index))
            expected[name] = digest(path)
        child = subprocess.Popen([cli, "import", "copy", destination, source, "--catalog", catalog],
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            deadline = time.monotonic() + 90
            while time.monotonic() < deadline and child.poll() is None:
                installed = len(list(destination.glob("*.DNG")))
                if 12 <= installed < 1000:
                    break
                time.sleep(0.05)
            else:
                raise AssertionError("No partially completed exFAT Copy to interrupt.")
        finally:
            if child.poll() is None:
                child.kill()
            child.wait(timeout=10)
        run(cli, catalog, "recover")
        retry = run(cli, catalog, "import", "copy", destination, source)
        assert retry["failed"] == 0, retry
        with closing(sqlite3.connect(catalog)) as db:
            assert db.execute("PRAGMA integrity_check").fetchone() == ("ok",)
            records = list(db.execute("SELECT original_path,checksum FROM assets"))
            assert len(records) == 1000
            for stored, checksum in records:
                copied = Path(stored)
                assert copied.parent == destination and digest(copied) == digest(source / copied.name) == expected[copied.name] == checksum
        moved = scratch / "moved"
        moved.mkdir()
        path = source / "Кадр-0000.DNG"
        move = run(cli, scratch / "move.sqlite", "import", "move", moved, path)
        assert move["added"] == 1 and move["failed"] == 0 and not path.exists(), move
        assert digest(moved / path.name) == expected[path.name]
        assert all(digest(source / name) == checksum for name, checksum in expected.items() if name != path.name)
    print("Windows exFAT Copy, interrupted import recovery, 1,000 hashes and cross-volume Move passed.")


if __name__ == "__main__":
    verify(*(Path(value).resolve(strict=True) for value in sys.argv[1:4]))
