"""Small PNG/dispatch infrastructure checks; no Qt or application child runs."""
import hashlib
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch
import zlib

import run_mask_transparent_regression as transparent


class TransparentMaskFixtureTests(unittest.TestCase):
    def test_one_literal_rgb_png_all_pixels_crc_sha_and_no_overwrite(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary).resolve() / 'fresh'
            hashes = transparent.one_gray_png(folder)
            source = folder / 'Photo-0000.png'
            self.assertEqual(list(folder.iterdir()), [source])
            self.assertEqual(hashes, {
                str(source): hashlib.sha256(source.read_bytes()).hexdigest(),
            })
            data = source.read_bytes()
            self.assertEqual(data[:8], b'\x89PNG\r\n\x1a\n')
            offset = 8
            kinds = []
            compressed = b''
            while offset < len(data):
                size = struct.unpack('>I', data[offset:offset + 4])[0]
                kind = data[offset + 4:offset + 8]
                payload = data[offset + 8:offset + 8 + size]
                crc = struct.unpack('>I', data[offset + 8 + size:offset + 12 + size])[0]
                self.assertEqual(zlib.crc32(kind + payload), crc)
                kinds.append(kind)
                if kind == b'IHDR':
                    self.assertEqual(
                        struct.unpack('>IIBBBBB', payload),
                        (256, 192, 8, 2, 0, 0, 0),
                    )
                elif kind == b'IDAT':
                    compressed += payload
                offset += 12 + size
            self.assertEqual(offset, len(data))
            self.assertEqual(kinds, [b'IHDR', b'IDAT', b'IEND'])
            decoded = zlib.decompress(compressed)
            self.assertEqual(len(decoded), 192 * (1 + 256 * 3))
            for y in range(192):
                row = decoded[y * 769:(y + 1) * 769]
                self.assertEqual(row[0], 0)
                for x in range(256):
                    expected = 32 + 3 * (x % 17) + 2 * (y % 11)
                    self.assertEqual(row[1 + 3*x:1 + 3*x + 3], bytes([expected]) * 3)
            self.assertEqual(transparent.runner.changed_originals(hashes), [])
            with self.assertRaises(ValueError):
                transparent.one_gray_png(folder)
            self.assertEqual(source.read_bytes(), data)

    def test_relative_existing_and_symlink_alias_directories_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            with self.assertRaises(ValueError):
                transparent.one_gray_png(Path('relative-fixture'))
            with self.assertRaises(ValueError):
                transparent.one_gray_png(root)
            outside = root / 'outside'
            outside.mkdir()
            alias = root / 'alias'
            alias.symlink_to(outside, target_is_directory=True)
            for path in (alias, alias / 'fresh'):
                with self.subTest(path=path), self.assertRaises(ValueError):
                    transparent.one_gray_png(path)
            self.assertEqual(list(outside.iterdir()), [])

    def test_deterministic_bytes_and_global_original_mutation_detection(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            first = transparent.one_gray_png(root / 'first')
            second = transparent.one_gray_png(root / 'second')
            self.assertEqual(set(first.values()), set(second.values()))
            path = Path(next(iter(first)))
            path.write_bytes(path.read_bytes() + b'changed')
            self.assertEqual(transparent.runner.changed_originals(first), [str(path)])
            self.assertEqual(transparent.runner.changed_originals(second), [])
            path.unlink()
            self.assertEqual(transparent.runner.changed_originals(first), [str(path)])

    def test_entrypoint_routes_only_own_fixture_supplemental_id(self):
        with patch.object(transparent.runner, 'main', return_value=17) as run:
            self.assertEqual(transparent.main(), 17)
        run.assert_called_once()
        options = run.call_args.kwargs
        self.assertEqual(options['domain'], 'supplemental')
        self.assertEqual(options['executable_ids'], {transparent.IDENTIFIER})
        self.assertIs(options['fixture_factory'], transparent.one_gray_png)
        self.assertEqual(options['default_report_name'], 'ux-mask-transparent-report')
        row, = options['inventory']
        self.assertEqual(
            (row['id'], row['scope'], row['mode'], row['severity']),
            (transparent.IDENTIFIER, 'CURRENT', 'REGRESSION', 'P1'),
        )
        self.assertEqual(transparent.IDENTIFIER, 'CANDIDATE-MSK023-TRANSPARENT-HANDLE')


if __name__ == '__main__':
    unittest.main()
