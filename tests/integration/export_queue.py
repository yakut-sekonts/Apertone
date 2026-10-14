"""Run immutable export queue acceptance without using the user's catalog."""
import hashlib
import json
import struct
import os
from pathlib import Path
import subprocess
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "assets"))
from generate_dng import create_dng
with tempfile.TemporaryDirectory(prefix="luma-export-queue-") as temporary:
    source = Path(temporary) / "Queue RAW.DNG"
    create_dng(source, maker="NIKON CORPORATION", model="NIKON D7500", width=512, height=384)
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    result = subprocess.run([sys.argv[1], str(source), str(Path(__file__).resolve().parents[2] / "apps/desktop/qml"), "--export-queue"], capture_output=True, text=True, env={**os.environ, "QT_QPA_PLATFORM": "offscreen", "QT_QUICK_BACKEND": "software"}, timeout=180)
    assert result.returncode == 0, result.stdout + result.stderr
    assert hashlib.sha256(source.read_bytes()).hexdigest() == digest
    print(result.stdout.strip())
    cli, app = Path(sys.argv[2]), Path(sys.argv[3])
    env = {**os.environ, "QT_QPA_PLATFORM": "offscreen", "QT_QUICK_BACKEND": "software"}
    def run(executable, *arguments):
        result = subprocess.run([str(executable), *map(str, arguments)], capture_output=True, text=True, env=env, timeout=120)
        assert result.returncode == 0, result.stdout + result.stderr
        assert "ReferenceError" not in result.stderr and "Unable to assign" not in result.stderr, result.stderr
        return result.stdout
    catalog = Path(temporary) / "cli.sqlite"
    run(cli, "open", source, "--catalog", catalog)
    for bits in (8, 16):
        output = Path(temporary) / f"metadata-{bits}.tif"
        run(cli, "export", output, "--catalog", catalog, "--format", f"tiff{bits}", "--compression", "zip", "--metadata", "all", "--color-space", "prophoto", "--resize", "width", "--size", "64")
        data = output.read_bytes(); endian = "<" if data[:2] == b"II" else ">"
        offset = struct.unpack_from(endian + "I", data, 4)[0]
        count = struct.unpack_from(endian + "H", data, offset)[0]; tags = {}
        for index in range(count):
            entry = offset + 2 + index * 12
            tag, kind, length, value = struct.unpack_from(endian + "HHII", data, entry)
            if tag in (256, 257, 258, 259):
                size = (2 if kind == 3 else 4) * length
                content = data[entry + 8:entry + 12] if size <= 4 else data[value:]
                tags[tag] = struct.unpack_from(endian + ("H" if kind == 3 else "I"), content)[0]
        assert tags[256] == 64 and tags[257] == 48 and tags[258] == bits and tags[259] in (8, 32946), tags
    output = Path(temporary) / "gui"; output.mkdir()
    run(app, "--catalog", Path(temporary) / "gui.sqlite", "--test-edit", source, "--test-export-queue", output)
    dng = next(output.glob("*.dng"))
    imported = json.loads(run(cli, "open", dng, "--catalog", Path(temporary) / "reimport.sqlite"))
    assert (imported["width"], imported["height"]) == (512, 384)
    assert hashlib.sha256(source.read_bytes()).hexdigest() == digest
    print("CLI TIFF depth/compression with source metadata and native QML queue → DNG reimport passed")
