"""Read-only Lightroom catalog migration, including a committed WAL."""

from contextlib import closing
import hashlib
import json
from pathlib import Path
import shutil
import sqlite3
import subprocess
import sys
import tempfile


SCHEMA = """
CREATE TABLE Adobe_variablesTable(name TEXT,value TEXT);
CREATE TABLE Adobe_images(id_local INTEGER PRIMARY KEY,id_global TEXT,masterImage INTEGER,
    copyName TEXT,rating INTEGER,pick INTEGER,colorLabels TEXT,fileFormat TEXT,rootFile INTEGER,
    captureTime TEXT DEFAULT '2020-01-01T12:00:00',touchTime REAL DEFAULT 0);
CREATE TABLE AgLibraryFile(id_local INTEGER PRIMARY KEY,folder INTEGER,baseName TEXT,extension TEXT);
CREATE TABLE AgLibraryFolder(id_local INTEGER PRIMARY KEY,pathFromRoot TEXT,rootFolder INTEGER);
CREATE TABLE AgLibraryRootFolder(id_local INTEGER PRIMARY KEY,absolutePath TEXT);
CREATE TABLE Adobe_imageDevelopSettings(image INTEGER,text TEXT);
CREATE TABLE AgLibraryCollection(id_local INTEGER PRIMARY KEY,name TEXT,creationId TEXT,parent INTEGER);
CREATE TABLE AgLibraryCollectionImage(id_local INTEGER PRIMARY KEY,collection INTEGER,
    image INTEGER,positionInCollection INTEGER);
CREATE TABLE AgLibraryCollectionContent(id_local INTEGER PRIMARY KEY,collection INTEGER,
    content TEXT,owningModule TEXT);
CREATE TABLE AgLibraryKeyword(id_local INTEGER PRIMARY KEY,name TEXT,parent INTEGER);
CREATE TABLE AgLibraryKeywordSynonym(keyword INTEGER,name TEXT);
CREATE TABLE AgLibraryKeywordImage(id_local INTEGER PRIMARY KEY,image INTEGER,tag INTEGER);
CREATE TABLE AgLibraryIPTC(id_local INTEGER PRIMARY KEY,image INTEGER,caption TEXT,copyright TEXT);
CREATE TABLE AgHarvestedIptcMetadata(id_local INTEGER PRIMARY KEY,image INTEGER,
    creatorRef INTEGER,locationRef INTEGER);
CREATE TABLE AgInternedIptcCreator(id_local INTEGER PRIMARY KEY,value TEXT);
CREATE TABLE AgInternedIptcLocation(id_local INTEGER PRIMARY KEY,value TEXT);
"""


def run(binary, *args, expected=0):
    child = subprocess.run([binary, *map(str, args)], capture_output=True, encoding="utf-8",
                           errors="replace", timeout=40)
    assert child.returncode == expected, (args[0], child.stdout, child.stderr)
    return json.loads(child.stdout) if child.stdout.strip() else None


def digest(file):
    return hashlib.sha256(file.read_bytes()).hexdigest()


def create_lightroom(file, originals):
    db = sqlite3.connect(file)
    assert db.execute("PRAGMA journal_mode=WAL").fetchone() == ("wal",)
    db.executescript(SCHEMA)
    db.execute("INSERT INTO Adobe_variablesTable VALUES('Adobe_storeProviderID',"
               "'a722796e-9acd-48e1-adf5-172b68149ccc')")
    db.execute("INSERT INTO AgLibraryRootFolder VALUES(1,?)", (originals.as_posix(),))
    db.execute("INSERT INTO AgLibraryFolder VALUES(1,'',1)")
    db.executemany("INSERT INTO AgLibraryFile VALUES(?,1,?,?)", [
        (1, "Кадр", "DNG"), (2, "Пропавший", "DNG"), (3, "Видео", "MP4")])
    db.executemany("INSERT INTO Adobe_images(id_local,id_global,masterImage,copyName,rating,pick,"
                   "colorLabels,fileFormat,rootFile) VALUES(?,?,?,?,?,?,?,?,?)", [
        (1, "11527b18-554f-40c3-ad34-ab0fccfa4971", None, None, 4, 1, "Green", "DNG", 1),
        (2, "f04bef6b-4fe4-4f7b-acc1-236212ed1617", 1, "Чёрно-белый", 2, 0, "Blue", "DNG", 1),
        (3, "37673186-bf99-4c50-89f8-fcea83020a04", None, None, 0, 0, "", "RAW", 2),
        (4, "fde7a05d-6909-4a00-946d-926b59f032cb", None, None, 0, 0, "", "VIDEO", 3),
    ])
    db.executemany("INSERT INTO Adobe_imageDevelopSettings VALUES(?,?)", [
        (1, 's={Exposure2012=1.25,ToneCurvePV2012={0,0,255,255},Texture=21,'
            'ColorGradeGlobalSat=19,GrainAmount=15,'
            'CameraProfile="User profile",MaskGroupBasedCorrections={{Active=true}}}'),
        (2, 's={Exposure2012=-0.75,WhiteBalance="As Shot"}'),
    ])
    db.executemany("INSERT INTO AgLibraryCollection VALUES(?,?,?,?)", [
        (1, "Портфолио", "com.adobe.ag.library.collection", 5),
        (2, "Отбор", "com.adobe.ag.library.smart_collection", None),
        (3, "Недавно правленные", "com.adobe.ag.library.smart_collection", None),
        (4, "Правило плагина", "com.adobe.ag.library.smart_collection", None),
        (5, "Архив", "com.adobe.ag.library.group", None)])
    db.executemany("INSERT INTO AgLibraryCollectionContent VALUES(?,?,?,'ag.library.smart_collection')", [
        (1, 2, 's={{criteria="rating",operation=">=",value=4},'
               '{criteria="keywords",operation="empty"},combine="union"}'),
        (2, 3, 's={{criteria="touchTime",operation="inLast",value=2,value_units="days"},'
               '{criteria="fileFormat",operation="==",value="DNG"},combine="intersect"}'),
        (3, 4, 's={{criteria="externalPlugin",operation="==",value="some plugin"},combine="intersect"}'),
    ])
    db.executemany("INSERT INTO AgLibraryCollectionImage VALUES(?,1,?,?)", [(1, 1, 1), (2, 2, 2)])
    db.executemany("INSERT INTO AgLibraryKeyword VALUES(?,?,?)", [(1, "Горы", 2), (2, "Природа", None)])
    db.execute("INSERT INTO AgLibraryKeywordSynonym VALUES(1,'Вершины')")
    db.execute("INSERT INTO AgLibraryKeywordImage VALUES(1,1,1)")
    db.execute("INSERT INTO AgLibraryIPTC VALUES(1,1,?,?)", ("Кадр из\nархива", "© Автор"))
    db.execute("INSERT INTO AgInternedIptcCreator VALUES(1,'Автор')")
    db.execute("INSERT INTO AgInternedIptcLocation VALUES(1,'Горный лагерь')")
    db.execute("INSERT INTO AgHarvestedIptcMetadata VALUES(1,1,1,1)")
    db.commit()
    return db


def verify(binary, folder):
    fixture = Path(__file__).resolve().parents[1] / "assets/migration-051/01-complete.DNG"
    originals = folder / "Исходники"
    originals.mkdir()
    raw = originals / "Кадр.DNG"
    shutil.copyfile(fixture, raw)
    source, target = folder / "Архив.lrcat", folder / "Каталог.sqlite"
    wal = source.with_name(source.name + "-wal")
    db = create_lightroom(source, originals)
    try:
        before = tuple(digest(path) for path in (source, wal, raw))
        dry_run = run(binary, "lightroom", "inspect", source)
        assert not target.exists()
        assert (dry_run["photos"], dry_run["available"], dry_run["missing"],
                dry_run["unsupported"], dry_run["virtualCopies"]) == (4, 2, 1, 1, 1), dry_run
        assert dry_run["partialEdits"] == 1 and dry_run["smartCollections"] == 3, dry_run
        assert dry_run["smartRulesMapped"] == 2, dry_run
        assert dry_run["mappedEdits"] == 2 and dry_run["unmappedEdits"] == 0, dry_run
        imported = run(binary, "lightroom", "import", source, "--catalog", target)
        assert (imported["added"], imported["copies"], imported["versions"],
                imported["collections"], imported["smartCollectionsImported"]) == (1, 1, 0, 3, 2), imported
        with closing(sqlite3.connect(target)) as db_target:
            assert db_target.execute("PRAGMA user_version").fetchone() == (26,)
            collections = dict(db_target.execute("SELECT name,id FROM collections"))
            assert set(collections) == {"Lightroom · Архив › Портфолио", "Lightroom · Отбор",
                                        "Lightroom · Недавно правленные"}
            assert db_target.execute("SELECT name FROM asset_creators").fetchone() == ("Автор",)
            assert db_target.execute("SELECT caption,copyright,location FROM asset_metadata "
                                     "WHERE caption<>''").fetchone() \
                == ("Кадр из\nархива", "© Автор", "Горный лагерь")
            assert set(db_target.execute("SELECT keyword FROM asset_keywords")) == {
                ("Горы",), ("Природа|Горы",), ("Вершины",)}
            assert db_target.execute("SELECT COUNT(*) FROM collection_assets").fetchone() == (2,)
            assert db_target.execute("SELECT COUNT(*) FROM lightroom_smart_rules").fetchone() == (3,)
            assert db_target.execute("SELECT COUNT(*) FROM lightroom_migration_photos").fetchone() == (2,)
            documents = db_target.execute("SELECT develop_document FROM lightroom_migration_photos").fetchall()
            assert any("User profile" in row[0] for row in documents)
        items = run(binary, "library", "--catalog", target)["items"]
        assert len(items) == 2, items
        original = next(item for item in items if not item["copyName"])
        copy_item = next(item for item in items if item["copyName"])
        assert (original["rating"], original["label"], copy_item["rating"], copy_item["label"]) \
            == (4, "green", 2, "blue"), items
        assert copy_item["path"] == original["path"] and copy_item["originalAsset"] == original["id"]
        smart, recent = (collections[f"Lightroom · {name}"] for name in ("Отбор", "Недавно правленные"))
        assert run(binary, "library", "--collection", smart, "--catalog", target)["total"] == 2
        assert run(binary, "library", "--collection", recent, "--catalog", target)["total"] == 0
        run(binary, "rate", "2", original["id"], "--catalog", target)
        assert run(binary, "library", "--collection", smart, "--catalog", target)["total"] == 1
        assert run(binary, "open", raw, "--catalog", target)["exposure"] == 1.25
        assert run(binary, "open-asset", copy_item["id"], "--catalog", target)["exposure"] == -0.75
        assert run(binary, "restore", "--catalog", target)["asset"] == copy_item["id"]
        run(binary, "exposure", "-1.5", "--catalog", target)
        assert run(binary, "library", "--collection", recent, "--catalog", target)["total"] == 1
        variant = run(binary, "copy", "create", copy_item["id"], "Вторая копия",
                      "--catalog", target)["id"]
        assert run(binary, "open-asset", variant, "--catalog", target)["exposure"] == -1.5
        run(binary, "copy", "rename", variant, "Переименована", "--catalog", target)
        assert run(binary, "library", "--search", "Переименована", "--catalog", target)["total"] == 1
        run(binary, "copy", "delete", variant, "--catalog", target)
        assert run(binary, "restore", "--catalog", target)["asset"] == original["id"]
        assert run(binary, "restore", "--catalog", target)["exposure"] == 1.25
        run(binary, "exposure", "0.5", "--catalog", target)
        again = run(binary, "lightroom", "import", source, "--catalog", target)
        assert again["unchanged"] == 2 and again["added"] == again["versions"] == 0, again
        assert run(binary, "restore", "--catalog", target)["exposure"] == 0.5
        renamed = folder / "Копия архива.lrcat"
        with closing(sqlite3.connect(renamed)) as copy:
            db.backup(copy)
        moved = run(binary, "lightroom", "import", renamed, "--catalog", target)
        assert moved["unchanged"] == 2 and moved["added"] == moved["versions"] == 0, moved
        run(binary, "lightroom", "import", source, "--catalog", source, expected=1)
        run(binary, "lightroom", "import", source, "--catalog", wal, expected=1)
        journal = source.with_name(source.name + "-journal")
        run(binary, "lightroom", "import", source, "--catalog", journal, expected=1)
        if sys.platform == "win32":
            case_alias = journal.with_name(journal.name.upper())
            run(binary, "lightroom", "import", source, "--catalog", case_alias, expected=1)
        assert not journal.exists()
        assert tuple(digest(path) for path in (source, wal, raw)) == before
    finally:
        db.close()


if __name__ == "__main__":
    cli = Path(sys.argv[1]).resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="apertone-lrcat-", dir=Path.cwd()) as folder:
        verify(cli, Path(folder))
