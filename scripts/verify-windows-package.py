"""Verify an extracted Windows package without the SDK on PATH."""

import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def verify_icons(exe):
    shell = ctypes.WinDLL("shell32")
    extract = shell.ExtractIconExW
    extract.argtypes = (
        wintypes.LPCWSTR, ctypes.c_int, ctypes.POINTER(wintypes.HICON),
        ctypes.POINTER(wintypes.HICON), wintypes.UINT,
    )
    extract.restype = wintypes.UINT
    destroy = ctypes.WinDLL("user32").DestroyIcon
    destroy.argtypes = (wintypes.HICON,)
    large, small = wintypes.HICON(), wintypes.HICON()
    try:
        count = extract(str(exe), -1, None, None, 0)
        extract(str(exe), 0, ctypes.byref(large), ctypes.byref(small), 1)
        if count != 1 or not large or not small:
            raise RuntimeError("Windows Shell could not extract both sizes of the Apertone icon.")
    finally:
        for icon in (large, small):
            if icon:
                destroy(icon)


def verify(package):
    source = Path(__file__).resolve().parents[1] / "tests/assets/migration-051/01-complete.DNG"
    desktop, cli = (package / name for name in ("Apertone.exe", "apertone-cli.exe"))
    verify_icons(desktop)
    windows = Path(os.environ["SystemRoot"])
    env = dict(os.environ, PATH=f"{windows / 'System32'};{windows}",
               QT_QPA_PLATFORM="windows", QT_QUICK_BACKEND="software")
    for variable in ("QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH", "QML_IMPORT_PATH"):
        env.pop(variable, None)
    with tempfile.TemporaryDirectory(prefix="apertone-native-", dir=Path.cwd()) as folder:
        state = Path(folder)
        catalog = state / "catalog.sqlite"

        def run(exe, *args, platform="windows"):
            child = subprocess.run([exe, *args, "--catalog", catalog], cwd=package,
                                   capture_output=True, timeout=90, env={**env, "QT_QPA_PLATFORM": platform},
                                   encoding="utf-8", errors="replace",
                                   creationflags=subprocess.CREATE_NO_WINDOW)
            if child.returncode:
                raise RuntimeError(f"{exe.name} returned {child.returncode}: {child.stderr}")
            return child.stdout

        photo = json.loads(run(cli, "open", source))
        assert (photo["width"], photo["height"]) == (128, 96)
        destination = state / "gain-map.jpg"
        run(cli, "export", destination, "--format", "jpeg-hdr")
        assert b"urn:iso:std:iso:ts:21496:-1" in destination.read_bytes()
        proof = state / "offscreen.pdf"
        run(cli, "deliver", "contact-sheet", proof, "Проверка Apertone", photo["asset"],
            platform="offscreen")
        content = proof.read_bytes()
        assert b"/FontFile2" in content and b"/ToUnicode" in content, "PDF captions lack an embedded font."
        run(desktop, "--smoke-test")
    print("Native Windows desktop, icons, command-line DNG, Ultra HDR JPEG and offscreen PDF verified.")


if __name__ == "__main__":
    verify(Path(sys.argv[1]).resolve(strict=True))
