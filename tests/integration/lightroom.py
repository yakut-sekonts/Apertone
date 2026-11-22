"""Read-only Lightroom catalog migration, including a committed WAL."""

from contextlib import closing
import hashlib
import json
from pathlib import Path
import shutil
import sqlite3
import struct
import subprocess
import sys
import tempfile
import zlib


SCHEMA = """
CREATE TABLE Adobe_variablesTable(name TEXT,value TEXT);
CREATE TABLE Adobe_images(id_local INTEGER PRIMARY KEY,id_global TEXT,masterImage INTEGER,
    copyName TEXT,rating INTEGER,pick INTEGER,colorLabels TEXT,fileFormat TEXT,rootFile INTEGER,
    captureTime TEXT DEFAULT '2020-01-01T12:00:00',touchTime REAL DEFAULT 0);
CREATE TABLE AgLibraryFile(id_local INTEGER PRIMARY KEY,folder INTEGER,baseName TEXT,extension TEXT);
CREATE TABLE AgLibraryFolder(id_local INTEGER PRIMARY KEY,pathFromRoot TEXT,rootFolder INTEGER);
CREATE TABLE AgLibraryRootFolder(id_local INTEGER PRIMARY KEY,absolutePath TEXT);
CREATE TABLE Adobe_imageDevelopSettings(image INTEGER,text TEXT,historySettingsID TEXT);
CREATE TABLE Adobe_libraryImageDevelopHistoryStep(id_local INTEGER PRIMARY KEY,image INTEGER,
    dateCreated REAL,name TEXT,text,id_global TEXT);
CREATE TABLE Adobe_libraryImageDevelopSnapshot(id_local INTEGER PRIMARY KEY,id_global TEXT,
    image INTEGER,name TEXT,text);
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


def compressed(text):
    data = text.encode("utf-8")
    return len(data).to_bytes(4, "big") + zlib.compress(data)


def tiff_header(magic, fields):
    field_end = 14 + 12 * len(fields)
    header = bytearray(struct.pack("<2sHIH", b"II", magic, 8, len(fields)))
    payload = bytearray()
    for tag, kind, count, data in sorted(fields):
        header += struct.pack("<HHI", tag, kind, count)
        if len(data) <= 4:
            header += data.ljust(4, b"\0")
        else:
            header += struct.pack("<I", field_end + len(payload))
            payload += data
    return header + bytes(4) + payload


def profile_fixture(path):
    model, name = b"Luma Synthetic Bayer Camera\0", b"User profile\0"
    matrix = b"".join(struct.pack("<ii", int(i in (0, 4, 8)), 1) for i in range(9))
    curve = struct.pack("<6f", 0, 0, 0.5, 0.37, 1, 1)
    path.write_bytes(tiff_header(0x4352, [
        (50708, 2, len(model), model), (50936, 2, len(name), name),
        (50778, 3, 1, struct.pack("<H", 21)), (50721, 10, 9, matrix),
        (50940, 11, 6, curve), (51109, 10, 1, struct.pack("<ii", 1, 4)),
    ]))


def bitmap_fixture():
    pixels = bytes([0, 0, 224, 255] * 4)
    fields = [(256, 4, 4), (257, 4, 4), (258, 3, 8), (259, 3, 1), (262, 3, 1),
              (277, 3, 1), (278, 4, 4), (279, 4, len(pixels))]
    fields.append((273, 4, 14 + 12 * (len(fields) + 1)))
    return b"\x02\0\0\0\x01" + bytes(11) + tiff_header(
        42, [(tag, kind, 1, struct.pack("<I" if kind == 4 else "<H", value))
             for tag, kind, value in fields]) + pixels


def verify_extras(binary, folder):
    folder.mkdir()
    raw = folder / "Кадр.DNG"
    shutil.copyfile(Path(__file__).resolve().parents[1] / "assets/migration-051/01-complete.DNG", raw)
    source, target = folder / "Source.lrcat", folder / "Apertone.sqlite"
    archive = source.with_suffix(".lrcat-data")
    archive.mkdir()
    packed = bitmap_fixture()
    key = hashlib.md5(packed).hexdigest()
    blob = archive / "000001.blob"
    profiles = folder / "Profiles"
    profiles.mkdir()
    profile = profiles / "User profile.dcp"
    profile_fixture(profile)
    db = create_lightroom(source, folder)
    try:
        settings = (
            's={Exposure2012=0.5,SharpenRadius=99,CameraProfile="User profile",'
            'MaskGroupBasedCorrections={{CorrectionName="Sky",LocalExposure2012=0.25,'
            'CorrectionMasks={{What="Mask/Image",Origin="2,1",WholeImageArea="0,0,8,8",'
            f'MaskDigest="{key}"'
            '}}},{CorrectionName="Gradient",LocalContrast2012=0.2,'
            'CorrectionMasks={{What="Mask/Gradient",FullX=0.25,FullY=0.25,'
            'ZeroX=0.75,ZeroY=0.75}}}}}'
        )
        db.execute("UPDATE Adobe_imageDevelopSettings SET text=?,historySettingsID=NULL WHERE image=1",
                   (settings,))
        db.commit()
        before = digest(source), digest(raw), digest(profile)
        initial = run(binary, "lightroom", "import", source, "--catalog", target)
        assert (initial["masksMapped"], initial["masksSkipped"], initial["profilesSkipped"]) == (1, 1, 1), initial
        assert run(binary, "open", raw, "--catalog", target)["exposure"] == 0.5
        run(binary, "exposure", "-0.35", "--catalog", target)

        frame = struct.pack("<II", 0x00248F37, 1) + bytes(22) + struct.pack("<QQ", 32, len(packed))
        blob.write_bytes(frame + bytes(16) + key.upper().encode("ascii") + packed)
        extras = ("lightroom", "import", source, "--profiles", profiles, "--catalog")
        update = run(binary, *extras, target)
        assert (update["masksMapped"], update["masksSkipped"], update["profilesMapped"]) == (2, 0, 1), update
        assert update["snapshots"] == 1 and update["unchanged"] == 2, update
        assert run(binary, "restore", "--catalog", target)["exposure"] == -0.35
        assert run(binary, *extras, target)["snapshots"] == 0

        fresh = folder / "Fresh.sqlite"
        assert run(binary, *extras, fresh)["masksMapped"] == 2
        run(binary, "open", raw, "--catalog", fresh)
        assert run(binary, "camera-profile", "show", "--catalog", fresh) == {
            "profile": "User profile", "digest": digest(profile)}
        with closing(sqlite3.connect(fresh)) as catalog:
            assert catalog.execute("SELECT count(*) FROM edit_local_masks").fetchone() == (2,)
            assert catalog.execute("SELECT digest FROM camera_profiles").fetchone() == (digest(profile),)
        edited, cleared = folder / "Edited.png", folder / "Default.png"
        run(binary, "export", edited, "--catalog", fresh)
        run(binary, "camera-profile", "clear", "--catalog", fresh)
        run(binary, "export", cleared, "--catalog", fresh)
        assert digest(edited) != digest(cleared)
        assert (digest(source), digest(raw), digest(profile)) == before
    finally:
        db.close()


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
    current_history = "5ab0db1a-c705-49ba-9cef-dd67fd3ab8f8"
    db.executemany("INSERT INTO Adobe_imageDevelopSettings VALUES(?,?,?)", [
        (1, 's={Exposure2012=1.25e0,ToneCurvePV2012={0,0,255,255},Texture=21,'
            'ColorGradeGlobalSat=19,GrainAmount=15,'
            'CameraProfile="User profile",MaskGroupBasedCorrections={{Active=true}}}', current_history),
        (2, 's={Exposure2012=-7.5e-1,WhiteBalance="As Shot"}', None),
    ])
    document = db.execute("SELECT text FROM Adobe_imageDevelopSettings WHERE image=1").fetchone()[0]
    db.executemany("INSERT INTO Adobe_libraryImageDevelopHistoryStep VALUES(?,?,?,?,?,?)", [
        (40, 1, 1, "$$$/Develop/Import=Import", compressed("s={Exposure2012=0}"), None),
        (41, 1, 2, "Выдержка", compressed(document), current_history),
        (42, 1, 3, "Later exposure", "s={Exposure2012=2.0}", None),
        (43, 1, 4, "Over budget", b"\xff\xff\xff\xff" + zlib.compress(b"s={Exposure2012=-2}"), None),
        (44, 2, 1, "Virtual copy", compressed('s={Exposure2012=-0.75,WhiteBalance="As Shot"}'), None),
        (45, 1, 5, "Same mapped controls", compressed(document), None),
    ])
    db.executemany("INSERT INTO Adobe_libraryImageDevelopSnapshot VALUES(?,?,?,?,?)", [
        (50, "ec58d54c-767c-45f8-a582-370c8849d2a2", 1, "Пейзаж", "s={Exposure2012=0.641}"),
        (51, "00272232-1d93-41c4-aee9-28f2623154b4", 1, "Пейзаж", compressed(document)),
        (52, "acd30efe-7490-460e-8f33-d37c593668ce", 2, "Копия", "s={Exposure2012=-0.278}"),
        (53, "1e45e235-7339-40f6-b113-a374a7222eb7", 1, "Unreadable", b"invalid"),
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
            assert db_target.execute("PRAGMA user_version").fetchone() == (28,)
            collections = {row['name']: row['id'] for row in run(binary, "collections", "--catalog", target)}
            assert set(collections) == {"Портфолио", "Lightroom · Отбор", "Lightroom · Недавно правленные"}
            sets = run(binary, "sets", "--catalog", target)
            assert len(sets) == 1 and sets[0]['path'] == "Lightroom · Архив"
            assert db_target.execute("SELECT set_id FROM collection_set_entries WHERE collection_id=?",
                                     (collections["Портфолио"],)).fetchone() == (sets[0]['id'],)
            assert db_target.execute("SELECT name FROM asset_creators").fetchone() == ("Автор",)
            assert db_target.execute("SELECT caption,copyright,location FROM asset_metadata "
                                     "WHERE caption<>''").fetchone() \
                == ("Кадр из\nархива", "© Автор", "Горный лагерь")
            assert set(db_target.execute("SELECT keyword FROM asset_keywords")) == {
                ("Горы",), ("Природа|Горы",), ("Вершины",)}
            assert db_target.execute("SELECT path,alias FROM keyword_aliases").fetchall() == [("Природа|Горы", "Вершины")]
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
        assert (imported["historyMapped"], imported["historySkipped"], imported["historySteps"]) == (5, 1, 5)
        assert (imported["snapshots"], imported["snapshotsPartial"], imported["snapshotsSkipped"]) == (3, 1, 1)
        with closing(sqlite3.connect(target)) as db_target:
            assert db_target.execute("SELECT action,is_current FROM edit_history_states WHERE asset_id=? ORDER BY position",
                                     (original['id'],)).fetchall() == [("Import", 0), ("Выдержка", 1),
                                                                     ("Later exposure", 0), ("Same mapped controls", 0)]
            assert db_target.execute("SELECT is_current FROM edit_history_states WHERE asset_id=?",
                                     (copy_item['id'],)).fetchall() == [(1,)]
            snapshots = {name: (asset, document) for name, asset, document in db_target.execute(
                "SELECT name,asset_id,document FROM edit_versions")}
            assert {name: asset for name, (asset, _) in snapshots.items()} == {
                "Пейзаж": original["id"], "Пейзаж (2)": original["id"], "Копия": copy_item["id"]}
            assert "0.641" in snapshots["Пейзаж (2)"][1] and "-0.278" in snapshots["Копия"][1]
            with db_target:
                db_target.execute("UPDATE edit_versions SET name='Переименованный снимок' WHERE name='Пейзаж'")
                db_target.execute("DELETE FROM edit_versions WHERE name='Пейзаж (2)'")
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
        assert again["unchanged"] == 2 and again["added"] == again["versions"] == again["historySteps"] == 0, again
        assert again["snapshots"] == 0, again
        with closing(sqlite3.connect(target)) as db_target:
            assert {name for (name,) in db_target.execute("SELECT name FROM edit_versions")} == {
                "Переименованный снимок", "Копия"}
        assert run(binary, "restore", "--catalog", target)["exposure"] == 0.5
        renamed = folder / "Копия архива.lrcat"
        with closing(sqlite3.connect(renamed)) as copy:
            db.backup(copy)
        moved = run(binary, "lightroom", "import", renamed, "--catalog", target)
        assert moved["unchanged"] == 2 and moved["added"] == moved["versions"] == 0, moved
        parent = run(binary, "set", "create", "Папка проекта", "--catalog", target)["id"]
        child = run(binary, "set", "create", "Этап", parent, "--catalog", target)["id"]
        run(binary, "set", "update", parent, "Петля", child, "--catalog", target, expected=1)
        run(binary, "set", "delete", parent, "--catalog", target, expected=1)
        moved_collection = collections["Портфолио"]
        run(binary, "collection", "move", moved_collection, child, "--catalog", target)
        assert run(binary, "library", "--collection", moved_collection, "--catalog", target)["total"] == 2
        run(binary, "collection", "create", "Портфолио", parent, "--catalog", target)
        run(binary, "collection", "create", "Портфолио", child, "--catalog", target, expected=1)
        run(binary, "keyword", "create", "Поездки|Север", "Полярный край", "--catalog", target)
        run(binary, "keyword", "add", "Поездки|Север", original["id"], "--catalog", target)
        assert run(binary, "library", "--search", "Полярный край", "--catalog", target)["total"] == 1
        run(binary, "keyword", "update", "Поездки", "Экспедиции", "--catalog", target)
        keywords = {row['path']: row for row in run(binary, "keywords", "--catalog", target)}
        assert keywords['Экспедиции|Север']['aliases'] == ['Полярный край']
        assert keywords['Экспедиции|Север']['photos'] == 1
        run(binary, "keyword", "delete", "Экспедиции", "--catalog", target)
        assert run(binary, "library", "--search", "Полярный край", "--catalog", target)["total"] == 0
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
        verify_extras(cli, Path(folder) / "Extras")
