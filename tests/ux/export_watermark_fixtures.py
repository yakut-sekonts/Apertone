"""Exact EXP028 input profile and child-local private runtime temporary directory."""
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import uuid
import zlib

OFFICIAL_CASE = 'UX-EXP-028'
PHOTO_NAMES = ('A-owned.png', 'B-owned.png', 'C-owned.png')
WATERMARK_NAME = 'Watermark-original.png'
PROFILE_NAME = 'export-watermark-snapshot-rgb4-v1'


def _png(path, width, height, variant):
    def pixel(x, y):
        if variant == 3:
            return (255, 0, 0)
        return (35 + 65 * variant + x % 13, 55 + y % 17, 70 + (x + y) % 11)
    def chunk(kind, data):
        return (struct.pack('>I', len(data)) + kind + data
                + struct.pack('>I', zlib.crc32(kind + data)))
    rows = b''.join(b'\0' + b''.join(bytes(pixel(x, y)) for x in range(width))
                    for y in range(height))
    encoded = (b'\x89PNG\r\n\x1a\n'
               + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
               + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))
    with path.open('xb') as output:
        output.write(encoded)


def official_fixture(folder, *, owned_root):
    folder, owned_root = Path(folder), Path(owned_root)
    if (not owned_root.is_absolute() or owned_root.resolve() != owned_root
            or not owned_root.is_dir() or owned_root.is_symlink()):
        raise ValueError('EXP028 profile requires a canonical owned run directory')
    if folder != owned_root / 'case-fixtures' / OFFICIAL_CASE:
        raise ValueError('EXP028 profile requires its exact dedicated official-case directory')
    if folder.resolve() != folder or folder.exists() or folder.is_symlink():
        raise ValueError('EXP028 profile requires a fresh directory without aliases')
    folder.parent.mkdir(exist_ok=True)
    folder.mkdir()
    for variant, name in enumerate(PHOTO_NAMES):
        _png(folder / name, 64, 48, variant)
    _png(folder / WATERMARK_NAME, 32, 32, 3)
    expected = sorted(folder / name for name in (*PHOTO_NAMES, WATERMARK_NAME))
    paths = sorted(folder.iterdir())
    if paths != expected or any(path.is_symlink() or not path.is_file() for path in paths):
        raise ValueError('EXP028 profile must contain exactly three RGB photos and one RGB mark')
    hashes = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}
    return hashes, {
        'case_id': OFFICIAL_CASE, 'name': PROFILE_NAME, 'directory': str(folder),
        'public_photo_count': 4, 'importable_photo_count': 3, 'watermark_fixture_count': 1,
        'original_checksum_count': 4, 'png_color_type': 'RGB',
        'photo_dimensions': {'width': 64, 'height': 48},
        'watermark_dimensions': {'width': 32, 'height': 32},
        'large_24mp_generated': False,
    }


def child_environment(environment, directory, fixture_folder, *, case_id):
    """Create only EXP028's owned temp sibling; return a copy for subprocess.Popen."""
    if case_id != OFFICIAL_CASE:
        raise ValueError('EXP028 launch fixture rejects unrelated official/supplemental IDs')
    directory, fixture_folder = Path(directory), Path(fixture_folder)
    run_root = fixture_folder.parent.parent
    if (not run_root.is_absolute() or run_root.resolve() != run_root
            or not run_root.is_dir() or run_root.is_symlink()
            or fixture_folder != run_root / 'case-fixtures' / OFFICIAL_CASE
            or fixture_folder.resolve() != fixture_folder or not fixture_folder.is_dir()
            or fixture_folder.is_symlink()):
        raise ValueError('EXP028 launch requires its canonical owned case profile')
    if (directory != run_root / OFFICIAL_CASE / directory.name
            or not re.fullmatch(r'repeat-[1-9][0-9]*', directory.name)
            or directory.resolve() != directory or not directory.is_dir()
            or directory.is_symlink()):
        raise ValueError('EXP028 launch requires this run exact canonical repeat directory')
    owned_temp = Path(str(directory) + '-owned-runtime-temp')
    if (owned_temp.resolve() != owned_temp or owned_temp.exists() or owned_temp.is_symlink()):
        raise ValueError('EXP028 launch temporary directory must be fresh without aliases')
    owned_temp.mkdir(mode=0o700)
    owned_temp.chmod(0o700)
    marker = {'case': case_id, 'case_root': str(directory), 'temp_root': str(owned_temp),
              'wrapper_pid': os.getpid(), 'nonce': str(uuid.uuid4())}
    with (owned_temp / 'wrapper-owner.json').open('x') as output:
        json.dump(marker, output)
        output.write('\n')
    return {**environment, 'TMPDIR': str(owned_temp),
            'LUMA_UX_EXP028_CASE_ROOT': str(directory), 'LUMA_UX_EXP028_TMP_ROOT': str(owned_temp)}
