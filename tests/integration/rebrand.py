"""Static contracts for the 1.1 Apertone rename and Luma upgrade path."""
from pathlib import Path
import plistlib


ROOT = Path(__file__).resolve().parents[2]


def require(text: str, needle: str, source: str) -> None:
    if needle not in text:
        raise AssertionError(f"{source} is missing {needle!r}")


info = plistlib.loads((ROOT / "platform/macos/Info.plist.in").read_bytes())
assert info["CFBundleIdentifier"] == "app.luma.desktop"
assert info["CFBundleName"] == info["CFBundleDisplayName"] == "Apertone"
assert info["CFBundleIconFile"] == "Apertone.icns"
assert info["SUFeedURL"] == (
    "https://github.com/yakut-sekonts/Apertone/releases/latest/download/appcast.xml"
)
assert info["SUPublicEDKey"] == "yyXDZkahzYddht4vWZjFW5oRBLFs4yCdkK7CZZk+SYI="

cmake = (ROOT / "CMakeLists.txt").read_text()
require(cmake, "project(Apertone VERSION 1.2.0", "CMakeLists.txt")
require(cmake, "qt_add_executable(Apertone MACOSX_BUNDLE", "CMakeLists.txt")
require(cmake, "URI Apertone.Desktop", "CMakeLists.txt")
require(cmake, "resources/branding/Apertone.icns", "CMakeLists.txt")

main = (ROOT / "apps/desktop/main.cpp").read_text()
require(main, 'setApplicationName(QStringLiteral("Apertone"))', "main.cpp")
require(main, 'setApplicationDisplayName(QStringLiteral("Apertone"))', "main.cpp")
require(main, 'setOrganizationName(QStringLiteral("Luma"))', "main.cpp")
assert main.index("parser.process(app)") < main.rindex(
    'setApplicationName(QStringLiteral("Luma"))'
)

decoder = (ROOT / "core/raw/RawDecoder.cpp").read_text()
require(decoder, 'make == "Apertone" || make == "Luma"', "RawDecoder.cpp")
require(decoder, 'model.starts_with("Luma Rendered Linear RGB")', "RawDecoder.cpp")

qml = (ROOT / "apps/desktop/qml/Main.qml").read_text()
require(qml, 'text: "Apertone"', "Main.qml")
require(qml, 'text: "APERTONE / DEVELOP CORE"', "Main.qml")
assert 'text: "luma"' not in qml
assert 'text: "LUMA / DEVELOP CORE"' not in qml

cli = (ROOT / "apps/cli/main.cpp").read_text()
require(cli, "Usage: apertone open RAW", "apps/cli/main.cpp")
assert "Usage: luma " not in cli

for script in ("package.sh", "package-dmg.sh", "release-artifacts.sh",
               "create-appcast.py", "publish-release.py"):
    source = (ROOT / "scripts" / script).read_text()
    require(source, "Apertone", script)

assert (ROOT / "resources/branding/Apertone.icns").stat().st_size > 100_000
assert (ROOT / "resources/branding/Apertone-1024.png").stat().st_size > 10_000
print("Apertone identity and Luma 1.0 upgrade compatibility passed")
