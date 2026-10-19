"""Exact bounded VIS003 input profile for the approved230 UTF-8-byte stem contract."""
import hashlib
from pathlib import Path
import struct
import zlib

OFFICIAL_CASE = 'UX-VIS-003'
PROFILE_NAME = 'export-current-filename-utf8-rgb4-v1'
WIDTH, HEIGHT = 64, 48
STEMS = ('Short-control', 'a' * 230, '\u044f' * 115, '\u044f' * 115 + 'a')
NAMES = tuple(stem + '.png' for stem in STEMS)
COLORS = ((40, 40, 48), (190, 40, 190), (40, 190, 190), (190, 190, 48))


def source_pixel(x, y, variant):
    """Literal independent RGB recipe, also transcribed into the C++ oracle."""
    if not 0 <= variant < len(COLORS):
        raise ValueError('Unknown VIS003 literal pixel variant')
    red, green, blue = COLORS[variant]
    return red + x % 13, green + y % 17, blue + (x + y) % 11


def write_png(path, variant):
    def chunk(kind, data):
        return (struct.pack('>I', len(data)) + kind + data
                + struct.pack('>I', zlib.crc32(kind + data)))
    rows = b''.join(b'\0' + b''.join(bytes(source_pixel(x, y, variant))
                                   for x in range(WIDTH)) for y in range(HEIGHT))
    encoded = (b'\x89PNG\r\n\x1a\n'
               + chunk(b'IHDR', struct.pack('>IIBBBBB', WIDTH, HEIGHT, 8, 2, 0, 0, 0))
               + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))
    with path.open('xb') as output:
        output.write(encoded)


def official_fixture(folder, *, owned_root):
    folder, owned_root = Path(folder), Path(owned_root)
    if (not owned_root.is_absolute() or owned_root.resolve() != owned_root
            or not owned_root.is_dir() or owned_root.is_symlink()):
        raise ValueError('VIS003 profile requires a canonical owned run directory')
    if folder != owned_root / 'case-fixtures' / OFFICIAL_CASE:
        raise ValueError('VIS003 profile requires its exact dedicated official-case directory')
    if folder.resolve() != folder or folder.exists() or folder.is_symlink():
        raise ValueError('VIS003 profile requires a fresh directory without aliases')
    if (tuple(len(stem.encode('utf-8')) for stem in STEMS) != (13, 230, 230, 231)
            or tuple(len(stem) for stem in STEMS) != (13, 230, 115, 116)
            or any(len(name.encode('utf-8')) > 255 for name in NAMES)):
        raise ValueError('VIS003 literal ASCII/multibyte filename boundaries changed')
    folder.parent.mkdir(exist_ok=True)
    folder.mkdir()
    for variant, name in enumerate(NAMES):
        write_png(folder / name, variant)
    paths = sorted(folder.iterdir())
    if (paths != sorted(folder / name for name in NAMES)
            or any(path.is_symlink() or not path.is_file() for path in paths)):
        raise ValueError('VIS003 profile requires exactly four new regular literal RGB PNG files')
    hashes = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}
    return hashes, {
        'case_id': OFFICIAL_CASE, 'name': PROFILE_NAME, 'directory': str(folder),
        'public_photo_count': 4, 'original_checksum_count': 4,
        'width': WIDTH, 'height': HEIGHT, 'png_color_type': 'RGB',
        'expanded_stem_utf8_bytes': [13, 230, 230, 231],
        'source_basename_utf8_bytes': [17, 234, 234, 235],
        'representative_output_format': 'PNG', 'all_formats_claimed': False,
        'large_24mp_generated': False,
    }
