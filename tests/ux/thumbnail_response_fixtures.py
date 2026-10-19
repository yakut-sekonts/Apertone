"""Small opaque palette sources for the actual thumbnail-response contract."""
import hashlib
from pathlib import Path
import struct
import zlib

OFFICIAL_CASE = 'UX-LIB-004'
RACE_CASE = 'UX-RACE-002'
AWAY_BACK_CASE = 'UX-LIB-005'
CASE_PROFILE_IDS = frozenset((OFFICIAL_CASE, RACE_CASE, AWAY_BACK_CASE))
PROFILE_NAME = 'thumbnail-response-rgb32-v1'
PHOTO_COUNT = 32
WIDTH, HEIGHT = 256, 192


def palette_png(path, index):
    if not 0 <= index < PHOTO_COUNT:
        raise ValueError('Thumbnail palette index must be in [0, 32)')
    # Opaque constant RGB; distinct files differ by >=70 in at least one channel.
    # The adapter's independent oracle also checks JPEG/scene separation.
    red = 40 + 70 * (index % 4)
    green = 40 + 70 * ((index // 4) % 4)
    blue = 48 + 128 * (index // 16)
    row = b'\0' + bytes((red, green, blue)) * WIDTH

    def chunk(kind, data):
        return (struct.pack('>I', len(data)) + kind + data +
                struct.pack('>I', zlib.crc32(kind + data)))

    encoded = (b'\x89PNG\r\n\x1a\n' +
               chunk(b'IHDR', struct.pack('>IIBBBBB', WIDTH, HEIGHT, 8, 2, 0, 0, 0)) +
               chunk(b'IDAT', zlib.compress(row * HEIGHT)) + chunk(b'IEND', b''))
    with Path(path).open('xb') as output:
        output.write(encoded)


def official_fixture(folder, *, owned_root, case_id=OFFICIAL_CASE):
    """Create the exact bounded RGB profile inside this run's case directory."""
    if case_id not in CASE_PROFILE_IDS:
        raise ValueError('Unknown official thumbnail fixture profile')
    profiles = {OFFICIAL_CASE: (PHOTO_COUNT, PROFILE_NAME),
                RACE_CASE: (2, 'thumbnail-invalidation-rgb2-v1'),
                AWAY_BACK_CASE: (PHOTO_COUNT, 'thumbnail-away-back-rgb32-v1')}
    count, profile_name = profiles[case_id]
    folder, owned_root = Path(folder), Path(owned_root)
    if (not owned_root.is_absolute() or owned_root.resolve() != owned_root
            or not owned_root.is_dir() or owned_root.is_symlink()):
        raise ValueError('Thumbnail profile requires a canonical owned run directory')
    if folder != owned_root / 'case-fixtures' / case_id:
        raise ValueError('Thumbnail profile must use its owned official-case directory')
    if folder.resolve() != folder or folder.exists() or folder.is_symlink():
        raise ValueError('Thumbnail profile requires a fresh directory without symlinks')
    folder.parent.mkdir(exist_ok=True)
    folder.mkdir()
    for index in range(count):
        palette_png(folder / f'Photo-{index:04}.png', index)
    paths = sorted(folder.iterdir())
    expected = [folder / f'Photo-{index:04}.png' for index in range(count)]
    if paths != expected or any(path.is_symlink() or not path.is_file() for path in paths):
        raise ValueError('Thumbnail profile file inventory differs from its exact bounded RGB PNG plan')
    hashes = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}
    return hashes, {
        'case_id': case_id, 'name': profile_name, 'directory': str(folder),
        'public_photo_count': len(paths), 'original_checksum_count': len(hashes),
        'width': WIDTH, 'height': HEIGHT, 'png_color_type': 'RGB',
        'large_24mp_generated': False,
    }
