"""Validate all embedded language catalogs without a translation-service dependency."""
import json
from pathlib import Path
import re
root = Path(__file__).resolve().parents[1]
directory = root / "resources/i18n"
codes = ("en", "ru", "zh_CN", "hy", "de", "es", "it", "fr")
source = json.loads((directory / "en.json").read_text())["messages"]
assert source and all(key == value for key, value in source.items())
for code in codes:
    pack = json.loads((directory / f"{code}.json").read_text())
    assert pack.keys() == {"schema", "locale", "messages"}
    assert pack["schema"] == 1 and pack["locale"] == code
    assert pack["messages"].keys() == source.keys(), code
    for key, value in pack["messages"].items():
        assert isinstance(value, str) and value and "\0" not in value, (code, key)
        for pattern in (r"%[1-9][0-9]*", r"\{(?:filename|sequence|date)\}", r"\*\.[a-z]+"):
            assert sorted(re.findall(pattern, key)) == sorted(re.findall(pattern, value)), (code, key)
        assert len(value.splitlines()) == len(key.splitlines()), (code, key)
for path in (root / "apps/desktop/qml").glob("*.qml"):
    for key in re.findall(r'qsTranslate\("Apertone",\s*("(?:[^"\\]|\\.)*")\)', path.read_text()):
        assert not json.loads(key) or json.loads(key) in source, (path, key)
    assert not re.search("[А-Яа-яЁё]", path.read_text()), f"Unmarked Russian source: {path}"
print(f"Eight catalogs: {len(source)} entries each; source coverage, placeholders and file tokens passed")
