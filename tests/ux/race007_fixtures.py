"""Exact three small opaque RGB sources for the partial-thumbnail/batch contract."""
import hashlib
from pathlib import Path
import struct
import zlib

OFFICIAL_CASE = 'UX-RACE-007'
PROFILE_NAME = 'thumbnail-partial-render-rgb3-v1'
COLORS = ((32, 2, 2), (2, 32, 2), (2, 2, 32))
WIDTH, HEIGHT = 256, 192


def _png(path, color):
    row = b'\0' + bytes(color) * WIDTH

    def chunk(kind, data):
        return (struct.pack('>I', len(data)) + kind + data
                + struct.pack('>I', zlib.crc32(kind + data)))

    encoded = (b'\x89PNG\r\n\x1a\n'
               + chunk(b'IHDR', struct.pack('>IIBBBBB', WIDTH, HEIGHT, 8, 2, 0, 0, 0))
               + chunk(b'IDAT', zlib.compress(row * HEIGHT)) + chunk(b'IEND', b''))
    with path.open('xb') as output:
        output.write(encoded)


def official_fixture(folder, *, owned_root):
    folder, owned_root = Path(folder), Path(owned_root)
    if (not owned_root.is_absolute() or owned_root.resolve() != owned_root
            or not owned_root.is_dir() or owned_root.is_symlink()):
        raise ValueError('RACE007 profile requires a canonical owned run directory')
    if folder != owned_root / 'case-fixtures' / OFFICIAL_CASE:
        raise ValueError('RACE007 profile requires its exact owned official-case directory')
    if folder.resolve() != folder or folder.exists() or folder.is_symlink():
        raise ValueError('RACE007 profile requires a fresh directory without aliases')
    folder.parent.mkdir(exist_ok=True)
    folder.mkdir()
    for index, color in enumerate(COLORS):
        _png(folder / f'Photo-{index:04d}.png', color)
    expected = [folder / f'Photo-{index:04d}.png' for index in range(len(COLORS))]
    paths = sorted(folder.iterdir())
    if paths != expected or any(path.is_symlink() or not path.is_file() for path in paths):
        raise ValueError('RACE007 profile must contain exactly three regular RGB PNGs')
    hashes = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}
    return hashes, {
        'case_id': OFFICIAL_CASE, 'name': PROFILE_NAME, 'directory': str(folder),
        'public_photo_count': 3, 'original_checksum_count': 3,
        'width': WIDTH, 'height': HEIGHT, 'png_color_type': 'RGB',
        'literal_colors': [list(color) for color in COLORS],
        'large_24mp_generated': False,
    }
