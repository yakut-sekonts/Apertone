"""One opaque striped source for the actual Crop Done composited-scene contract."""
import hashlib
from pathlib import Path
import struct
import zlib

OFFICIAL_CASE = 'UX-CRP-010'
PROFILE_NAME = 'crop-done-stripes-rgb1-v1'
WIDTH, HEIGHT = 256, 192


def official_fixture(folder, *, owned_root):
    folder, owned_root = Path(folder), Path(owned_root)
    if (not owned_root.is_absolute() or owned_root.resolve() != owned_root
            or not owned_root.is_dir() or owned_root.is_symlink()):
        raise ValueError('CRP010 profile requires a canonical owned run directory')
    if folder != owned_root / 'case-fixtures' / OFFICIAL_CASE:
        raise ValueError('CRP010 profile requires its exact owned official-case directory')
    if folder.resolve() != folder or folder.exists() or folder.is_symlink():
        raise ValueError('CRP010 profile requires a fresh directory without aliases')
    folder.parent.mkdir(exist_ok=True)
    folder.mkdir()
    channels = [255 if (x // 64) % 2 == 0 else 0 for x in range(WIDTH)]
    row = b'\0' + b''.join(bytes((value, value, value)) for value in channels)

    def chunk(kind, data):
        return (struct.pack('>I', len(data)) + kind + data +
                struct.pack('>I', zlib.crc32(kind + data)))

    encoded = (b'\x89PNG\r\n\x1a\n' +
               chunk(b'IHDR', struct.pack('>IIBBBBB', WIDTH, HEIGHT, 8, 2, 0, 0, 0)) +
               chunk(b'IDAT', zlib.compress(row * HEIGHT)) + chunk(b'IEND', b''))
    source = folder / 'Photo-0000.png'
    with source.open('xb') as output:
        output.write(encoded)
    if list(folder.iterdir()) != [source] or not source.is_file() or source.is_symlink():
        raise ValueError('CRP010 profile must contain exactly one regular striped RGB PNG')
    hashes = {str(source): hashlib.sha256(source.read_bytes()).hexdigest()}
    return hashes, {
        'case_id': OFFICIAL_CASE, 'name': PROFILE_NAME, 'directory': str(folder),
        'public_photo_count': 1, 'original_checksum_count': 1,
        'width': WIDTH, 'height': HEIGHT, 'png_color_type': 'RGB',
        'stripe_width': 64, 'first_stripe_rgb': [255, 255, 255],
        'large_24mp_generated': False,
    }
