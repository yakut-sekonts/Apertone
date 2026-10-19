"""Bounded official RACE002 profile infrastructure; no native application or UI."""
import hashlib
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

import quality_policy as policy
import run_ux_suite as runner
import test_thumbnail_response_fixtures as response_tests
import thumbnail_response_fixtures as palette


class ThumbnailInvalidationFixtureTests(unittest.TestCase):
    # Reuse the existing mocked runner harness, not its TestCase or UX assertions.
    run_cli = response_tests.ThumbnailFixtureTests.run_cli

    def test_exact_two_literal_rgb_colors_and_manifest(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            folder = root / 'case-fixtures' / palette.RACE_CASE
            hashes, manifest = palette.official_fixture(folder, owned_root=root,
                                                        case_id=palette.RACE_CASE)
            self.assertEqual(sorted(Path(name).name for name in hashes),
                             ['Photo-0000.png', 'Photo-0001.png'])
            self.assertEqual(manifest['case_id'], 'UX-RACE-002')
            self.assertEqual(manifest['name'], 'thumbnail-invalidation-rgb2-v1')
            self.assertEqual((manifest['public_photo_count'], manifest['original_checksum_count']), (2, 2))
            self.assertEqual((manifest['width'], manifest['height'], manifest['png_color_type']),
                             (256, 192, 'RGB'))
            self.assertFalse(manifest['large_24mp_generated'])
            for filename, color in [('Photo-0000.png', (40, 40, 48)),
                                    ('Photo-0001.png', (110, 40, 48))]:
                path = folder / filename
                data = path.read_bytes()
                self.assertEqual(hashlib.sha256(data).hexdigest(), hashes[str(path)])
                self.assertEqual(data[:8], b'\x89PNG\r\n\x1a\n')
                self.assertEqual(struct.unpack('>IIBBBBB', data[16:29]),
                                 (256, 192, 8, 2, 0, 0, 0))
                offset, compressed = 8, b''
                while offset < len(data):
                    size = struct.unpack('>I', data[offset:offset + 4])[0]
                    kind, payload = data[offset + 4:offset + 8], data[offset + 8:offset + 8 + size]
                    self.assertEqual(zlib.crc32(kind + payload),
                                     struct.unpack('>I', data[offset + 8 + size:offset + 12 + size])[0])
                    if kind == b'IDAT':
                        compressed += payload
                    offset += size + 12
                self.assertEqual(zlib.decompress(compressed), (b'\0' + bytes(color) * 256) * 192)
            with self.assertRaises(ValueError):
                palette.official_fixture(folder, owned_root=root, case_id=palette.RACE_CASE)
            self.assertEqual(runner.changed_originals(hashes), [])

    def test_solo_profile_routes_two_files_in_both_modes_without_default_load(self):
        for quality in (False, True):
            with self.subTest(quality=quality):
                outcome = self.run_cli([palette.RACE_CASE], quality=quality, runs=3)
                self.assertEqual(outcome['code'], 0)
                self.assertEqual((outcome['base_calls'], outcome['extra_calls']), (0, 1))
                self.assertEqual(len(outcome['calls']), 3)
                self.assertEqual(len({str(folder) for _, folder in outcome['calls']}), 1)
                self.assertTrue(all(folder.parts[-2:] == ('case-fixtures', 'UX-RACE-002')
                                    for _, folder in outcome['calls']))
                summary = outcome['summary']
                self.assertEqual(summary['inventory_domain'], 'specification')
                self.assertEqual(summary['release_gate'], 'NOT_QUALIFIED')
                profile = summary['fixture_profile']
                self.assertEqual((profile['public_photo_count'], profile['extra_public_photo_count'],
                                  profile['original_checksum_count']), (0, 2, 2))
                self.assertEqual(len(profile['case_profiles']), 1)
                self.assertEqual(profile, outcome['manifest']['fixture_profile'])
                self.assertEqual(len(outcome['manifest']['original_sha256']), 2)

    def test_both_official_profiles_have_separate_inputs_and_union34_once(self):
        outcome = self.run_cli([palette.OFFICIAL_CASE, palette.RACE_CASE, 'UX-UND-018'], runs=3)
        self.assertEqual(outcome['code'], 0)
        self.assertEqual((outcome['base_calls'], outcome['extra_calls']), (0, 2))
        self.assertEqual(len(outcome['calls']), 6)
        folders = {identifier: {str(folder) for case, folder in outcome['calls'] if case == identifier}
                   for identifier in (palette.OFFICIAL_CASE, palette.RACE_CASE)}
        self.assertTrue(all(len(paths) == 1 for paths in folders.values()))
        self.assertFalse(folders[palette.OFFICIAL_CASE].intersection(folders[palette.RACE_CASE]))
        profile = outcome['summary']['fixture_profile']
        self.assertEqual((profile['public_photo_count'], profile['extra_public_photo_count'],
                          profile['original_checksum_count']), (0, 34, 34))
        self.assertEqual({item['case_id']: item['public_photo_count'] for item in profile['case_profiles']},
                         {'UX-LIB-004': 32, 'UX-RACE-002': 2})
        self.assertEqual(len(outcome['manifest']['original_sha256']), 34)
        integrity = [row for row in outcome['summary']['results'] if row['id'] == 'UX-UND-018']
        self.assertEqual(len(integrity), 3)
        self.assertTrue(all(row['original_count'] == 34 and row['status'] == 'PASS' for row in integrity))

    def test_generic_cases_keep_their_existing_fixture_and_counts(self):
        for identifier, count in [('UX-RACE-005', 1), ('UX-SEL-002', 10), ('UX-IMP-009', 20)]:
            with self.subTest(identifier=identifier):
                outcome = self.run_cli([palette.RACE_CASE, identifier])
                self.assertEqual(outcome['code'], 0)
                self.assertEqual((outcome['base_calls'], outcome['extra_calls']), (1, 1))
                self.assertEqual(outcome['base_options'], {'count': count, 'include_large': False})
                folders = dict(outcome['calls'])
                self.assertEqual(folders[identifier].name, 'public-fixtures-quality')
                self.assertEqual(folders[palette.RACE_CASE].name, 'UX-RACE-002')
                self.assertFalse(folders[palette.RACE_CASE].is_relative_to(folders[identifier]))
                profile = outcome['summary']['fixture_profile']
                self.assertEqual((profile['public_photo_count'], profile['base_original_checksum_count'],
                                  profile['extra_public_photo_count'], profile['original_checksum_count']),
                                 (count, count + 2, 2, count + 4))
                self.assertEqual(len(outcome['manifest']['original_sha256']), count + 4)
        outcome = self.run_cli([palette.OFFICIAL_CASE, palette.RACE_CASE, 'UX-SEL-002'])
        self.assertEqual(outcome['summary']['fixture_profile']['original_checksum_count'], 46)
        self.assertEqual(outcome['base_options'], {'count': 10, 'include_large': False})

    def test_ineligible_profile_only_requests_never_generate_base_or_extra(self):
        options_list = ({'native': False}, {'override_row': {'scope': '0.6'}},
                        {'extra_flags': ('--suite', 'blocking')},
                        {'extra_flags': ('--require-build-provenance',), 'provenance': 'FAIL'})
        for options in [dict(options, quality=quality) for options in options_list
                        for quality in (False, True)] + [{'denied': True}]:
            with self.subTest(options=options):
                outcome = self.run_cli([palette.RACE_CASE], **options)
                self.assertNotEqual(outcome['code'], 0)
                self.assertEqual((outcome['base_calls'], outcome['extra_calls']), (0, 0))
                self.assertEqual(outcome['calls'], [])
                if outcome['summary'] is not None:
                    self.assertEqual(outcome['summary']['release_gate'], 'NOT_QUALIFIED')
                    self.assertEqual(outcome['summary']['execution_status'], 'INCOMPLETE')
                    self.assertFalse(any(row['status'] == 'PASS' for row in outcome['summary']['results']))
        outcome = self.run_cli([palette.OFFICIAL_CASE, palette.RACE_CASE], quality=False,
                               extra_flags=('--suite', 'blocking'))
        self.assertNotEqual(outcome['code'], 0)
        self.assertEqual((outcome['base_calls'], outcome['extra_calls'], len(outcome['calls'])), (0, 0, 0))

    def test_race_original_mutation_removal_or_same_bytes_symlink_fail(self):
        for mutation in ('bytes', 'remove', 'symlink'):
            with self.subTest(mutation=mutation):
                outcome = self.run_cli([palette.RACE_CASE], mutate=mutation,
                                       mutation_case=palette.RACE_CASE)
                self.assertEqual(outcome['code'], 1)
                self.assertEqual(outcome['summary']['original_hashes'], 'FAIL')
                self.assertEqual(outcome['summary']['results'][0]['original_integrity'], 'FAIL')
                self.assertEqual(len(outcome['summary']['results'][0]['mutated_originals']), 1)
        outcome = self.run_cli([palette.RACE_CASE, 'UX-UND-018'], runs=3)
        self.assertEqual(outcome['code'], 0)
        integrity = [row for row in outcome['summary']['results'] if row['id'] == 'UX-UND-018']
        self.assertEqual(len(integrity), 3)
        self.assertTrue(all(row['original_count'] == 2 for row in integrity))

    def test_profile_cannot_alias_another_case_or_escape_owned_directory(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            for case_id, folder in [('unknown', root / 'case-fixtures/unknown'),
                                    (palette.RACE_CASE, root / 'case-fixtures' / palette.OFFICIAL_CASE),
                                    (palette.OFFICIAL_CASE, root / 'case-fixtures' / palette.RACE_CASE)]:
                with self.subTest(case_id=case_id, folder=folder), self.assertRaises(ValueError):
                    palette.official_fixture(folder, owned_root=root, case_id=case_id)
            self.assertFalse((root / 'case-fixtures').exists())

    def test_policy_inventory_and_ci_keep_the_official_domain(self):
        row = next(row for row in runner.INVENTORY if row['id'] == palette.RACE_CASE)
        self.assertEqual((row['scope'], row['mode'], row['severity']), ('CURRENT', 'REGRESSION', 'P1'))
        self.assertIn(row['id'], runner.NATIVE)
        self.assertIsNone(policy.quality_skip_reason(row))
        for filename in ('macos.yml', 'release.yml'):
            workflow = (runner.ROOT / '.github/workflows' / filename).read_text()
            self.assertIn('run_ux_suite.py --build build --quality-only --case UX-RACE-002 --runs 3', workflow)
            self.assertIn('build/ux-thumbnail-invalidation-report/', workflow)
            self.assertIn('build/ux-artifacts/20*/**/race002-*.json', workflow)
            self.assertNotIn('build/ux-artifacts/20*/case-fixtures/', workflow)


if __name__ == '__main__':
    unittest.main()
