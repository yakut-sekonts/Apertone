"""Qualify the actual Windows updater against disposable portable installations."""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time


FIXTURE = r"""
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <string_view>

int wmain(int argc, wchar_t **argv) {
    if (argc == 4 && std::wstring_view(argv[1]) == L"--wait") {
        std::ofstream(std::filesystem::path(argv[2])).put('1');
        for (int attempt = 0; attempt < 1000; ++attempt) {
            if (GetFileAttributesW(argv[3]) != INVALID_FILE_ATTRIBUTES)
                return 0;
            Sleep(10);
        }
        return 3;
    }
    if (argc != 3 || std::wstring_view(argv[1]) != L"--catalog")
        return 2;
    auto marker = std::filesystem::path(argv[2]);
    marker += L".started";
    std::ifstream identity("revision.txt", std::ios::binary);
    std::ofstream result(marker, std::ios::binary);
    result << identity.rdbuf();
    return result ? 0 : 4;
}
"""


def make_package(folder, binary, revision):
    folder.mkdir()
    if binary is None:
        (folder / "Apertone.exe").write_bytes(b"invalid portable executable")
    else:
        shutil.copyfile(binary, folder / "Apertone.exe")
    (folder / "BUILD-ORIGIN.txt").write_text(
        "Repository: https://github.com/yakut-sekonts/Apertone\n"
        f"Commit: {revision * 40}\nPlatform: Windows x64\n",
        encoding="utf-8",
    )
    (folder / "revision.txt").write_text(revision, encoding="utf-8")
    (folder / "tools").mkdir()
    (folder / "tools" / "payload.txt").write_text("fixture", encoding="utf-8")
    (folder / "UPDATE-FILES.txt").write_text(
        "Apertone.exe\nBUILD-ORIGIN.txt\nrevision.txt\ntools/payload.txt\n",
        encoding="utf-8",
    )


def update(helper, *args):
    return subprocess.run([helper, *args], timeout=15, check=False).returncode


def verify(helper, compiler):
    with tempfile.TemporaryDirectory(prefix="apertone-update-fixture-") as temporary:
        base = Path(temporary).resolve(strict=True)
        source = base / "fixture.cpp"
        fixture = base / "fixture.exe"
        source.write_text(FIXTURE, encoding="utf-8")
        compiler_env = {**os.environ, "PATH": f"{compiler.parent};{os.environ['PATH']}"}
        subprocess.run(
            [compiler, "-std=c++20", "-municode", "-mwindows", "-static", "-o", fixture, source],
            env=compiler_env, check=True, timeout=30,
        )
        install = base / "Apertone \u0424\u043e\u0442\u043e & owner's copy"
        stage = base / ".apertone-update-abc123"
        stage.mkdir()
        staged_helper = Path(shutil.copyfile(helper, stage / "ApertoneUpdate.exe"))
        package = stage / "package"
        make_package(install, fixture, "a")
        make_package(package, fixture, "b")
        catalog = base / "My \u0424\u043e\u0442\u043e & catalog.sqlite"
        catalog.write_bytes(b"catalog stays outside the program swap")

        assert update(staged_helper, "--validate", install) == 0
        foreign = install / "user-catalog.sqlite"
        foreign.write_bytes(b"user data must block a directory swap")
        assert update(staged_helper, "--validate", install) == 1
        assert update(staged_helper, install, "0", catalog) == 1
        assert foreign.read_bytes() == b"user data must block a directory swap"
        foreign.unlink()

        listing = package / "UPDATE-FILES.txt"
        original_listing = listing.read_bytes()
        listing.write_bytes(original_listing + b"../not-owned.txt\n")
        assert update(staged_helper, "--validate", install) == 1
        listing.write_bytes(original_listing + b"REVISION.TXT\n")
        assert update(staged_helper, "--validate", install) == 1
        listing.write_bytes(original_listing)

        ready = base / "old-ready"
        release = base / "old-may-close"
        old = subprocess.Popen([install / "Apertone.exe", "--wait", ready, release])
        engine = None
        try:
            deadline = time.monotonic() + 5
            while not ready.exists() and old.poll() is None and time.monotonic() < deadline:
                time.sleep(0.025)
            assert ready.exists() and old.poll() is None, "Old Apertone fixture failed to start"
            engine = subprocess.Popen([staged_helper, install, str(old.pid), catalog])
            time.sleep(0.25)
            assert engine.poll() is None, "Helper failed to wait for the old process"
            assert (install / "revision.txt").read_text(encoding="utf-8") == "a"
            release.touch()
            assert old.wait(timeout=5) == 0
            assert engine.wait(timeout=10) == 0
        finally:
            for process in (engine, old):
                if process is not None and process.poll() is None:
                    process.terminate()
                    process.wait(timeout=5)

        assert (install / "revision.txt").read_text(encoding="utf-8") == "b"
        assert Path(f"{catalog}.started").read_bytes() == b"b"
        assert catalog.read_bytes() == b"catalog stays outside the program swap"
        assert not package.exists()
        assert not (stage / "previous").exists()
        assert not (base / ".apertone-update.lock").exists()

        make_package(package, None, "c")
        assert update(staged_helper, "--validate", install) == 0
        assert update(staged_helper, install, "0", catalog) == 1
        assert (install / "revision.txt").read_text(encoding="utf-8") == "b"
        assert (package / "revision.txt").read_text(encoding="utf-8") == "c"
        assert catalog.read_bytes() == b"catalog stays outside the program swap"
        assert not (stage / "previous").exists()
        assert not (base / ".apertone-update.lock").exists()
    print("Windows updater preflight, wait, swap, quoted relaunch, and rollback passed.")


if __name__ == "__main__":
    verify(*(Path(value).resolve(strict=True) for value in sys.argv[1:3]))
