"""Prospective VIS003 profile accounting: tiny owned files/mocked children, no app."""
import hashlib
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import export_current_filename_fixtures as filename
import export_watermark_fixtures as watermark
import quality_policy as policy
import run_ux_suite as runner
import test_export_watermark_fixtures as png_tests
import test_thumbnail_response_fixtures as response_tests
import thumbnail_response_fixtures as palette


class ExportCurrentFilenameFixtureTests(unittest.TestCase):

    def run_cli(self, *args, **kwargs):
        with patch.object(
            runner,
            'current_filename_fixture',
            wraps=filename.official_fixture
        ) as factory:
            result = response_tests.ThumbnailFixtureTests.run_cli(self, *args, **kwargs)
            result['filename_calls'] = factory.call_count
            return result

    def test_exact_four_literal_pngs_utf8_boundaries_crc_pixels_and_manifest(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            folder = root / 'case-fixtures' / filename.OFFICIAL_CASE
            hashes, profile = filename.official_fixture(folder, owned_root=root)
            names = (
                'Short-control.png',
                'a' * 230 + '.png',
                'я' * 115 + '.png',
                'я' * 115 + 'a.png'
            )
            self.assertEqual(set((path.name for path in folder.iterdir())), set(names))
            self.assertEqual(tuple(map(len, filename.STEMS)), (13, 230, 115, 116))
            self.assertEqual(
                tuple((len(stem.encode('utf-8')) for stem in filename.STEMS)),
                (13, 230, 230, 231)
            )
            self.assertEqual(
                tuple((len(name.encode('utf-8')) for name in names)),
                (17, 234, 234, 235)
            )
            self.assertEqual(
                (len(hashes), profile['public_photo_count'], profile['original_checksum_count']),
                (4, 4, 4)
            )
            self.assertEqual(
                (profile['width'], profile['height'], profile['png_color_type']),
                (64, 48, 'RGB')
            )
            self.assertEqual(profile['name'], 'export-current-filename-utf8-rgb4-v1')
            self.assertEqual(profile['expanded_stem_utf8_bytes'], [13, 230, 230, 231])
            self.assertEqual(profile['source_basename_utf8_bytes'], [17, 234, 234, 235])
            self.assertEqual(profile['representative_output_format'], 'PNG')
            self.assertFalse(profile['all_formats_claimed'])
            self.assertFalse(profile['large_24mp_generated'])
            bases = ((40, 40, 48), (190, 40, 190), (40, 190, 190), (190, 190, 48))
            for variant, name in enumerate(names):
                path = folder / name
                self.assertFalse(path.is_symlink())
                self.assertEqual(hashes[str(path)], hashlib.sha256(path.read_bytes()).hexdigest())
                header, pixels = png_tests.decode_rgb_png(path.read_bytes())
                self.assertEqual(header, (64, 48, 8, 2, 0, 0, 0))
                expected_rows = []
                base = bases[variant]
                for y in range(48):
                    row = bytearray(b'\0')
                    for x in range(64):
                        row.extend((base[0] + x % 13, base[1] + y % 17,
                                    base[2] + (x + y) % 11))
                    expected_rows.append(bytes(row))
                expected = b''.join(expected_rows)
                self.assertEqual(pixels, expected)
            self.assertEqual(len(set(hashes.values())), 4)
            self.assertEqual(runner.changed_originals(hashes), [])
            before = {path.name: path.read_bytes() for path in folder.iterdir()}
            with self.assertRaises(ValueError):
                filename.official_fixture(folder, owned_root=root)
            self.assertEqual({path.name: path.read_bytes() for path in folder.iterdir()}, before)

    def test_factory_rejects_foreign_relative_alias_symlink_and_unknown_roots(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            folder = root / 'case-fixtures' / filename.OFFICIAL_CASE
            for wrong in (
                root / 'foreign',
                root / 'case-fixtures' / palette.OFFICIAL_CASE,
                root / 'case-fixtures' / 'CANDIDATE-VIS003-LEGAL-CURRENT-FILENAME'
            ):
                with self.subTest(wrong=wrong), self.assertRaises(ValueError):
                    filename.official_fixture(wrong, owned_root=root)
            with self.assertRaises(ValueError):
                filename.official_fixture(
                    Path('case-fixtures') / filename.OFFICIAL_CASE,
                    owned_root=Path('.')
                )
            outside = root / 'outside'
            outside.mkdir()
            alias = root / 'alias'
            alias.symlink_to(root, target_is_directory=True)
            with self.assertRaises(ValueError):
                filename.official_fixture(
                    alias / 'case-fixtures' / filename.OFFICIAL_CASE,
                    owned_root=alias
                )
            folder.parent.symlink_to(outside, target_is_directory=True)
            with self.assertRaises(ValueError):
                filename.official_fixture(folder, owned_root=root)
            self.assertEqual(list(outside.iterdir()), [])

    def test_solo_both_modes_four_sources_once_for_three_mock_children(self):
        for quality in (False, True):
            with self.subTest(quality=quality):
                result = self.run_cli([filename.OFFICIAL_CASE], quality=quality, runs=3)
                self.assertEqual(
                    (
                        result['code'],
                        result['base_calls'],
                        result['extra_calls'],
                        result['filename_calls']
                    ),
                    (0, 0, 0, 1)
                )
                self.assertEqual(len(result['calls']), 3)
                self.assertEqual(len({str(folder) for _, folder in result['calls']}), 1)
                self.assertTrue(all((
                    folder.parts[-2:] == ('case-fixtures', 'UX-VIS-003') for _,
                    folder in result['calls']
                )))
                profile = result['summary']['fixture_profile']
                self.assertEqual(
                    (
                        profile['public_photo_count'],
                        profile['base_original_checksum_count'],
                        profile['extra_public_photo_count'],
                        profile['original_checksum_count']
                    ),
                    (0, 0, 4, 4)
                )
                self.assertFalse(profile['large_24mp_generated'])
                self.assertEqual(profile, result['manifest']['fixture_profile'])
                self.assertEqual(len(result['manifest']['original_sha256']), 4)
                self.assertEqual(
                    (result['summary']['inventory_domain'], result['summary']['release_gate']),
                    ('specification', 'NOT_QUALIFIED')
                )

    def test_all_five_profiles_union74_and_global_integrity_no_generic(self):
        identifiers = [
            palette.OFFICIAL_CASE,
            palette.AWAY_BACK_CASE,
            palette.RACE_CASE,
            watermark.OFFICIAL_CASE,
            filename.OFFICIAL_CASE,
            'UX-UND-018'
        ]
        result = self.run_cli(identifiers, runs=3)
        self.assertEqual(
            (result['code'], result['base_calls'], result['extra_calls'], result['filename_calls']),
            (0, 0, 3, 1)
        )
        self.assertEqual(len(result['calls']), 15)
        profile = result['summary']['fixture_profile']
        self.assertEqual(
            (
                profile['public_photo_count'],
                profile['base_original_checksum_count'],
                profile['extra_public_photo_count'],
                profile['original_checksum_count']
            ),
            (0, 0, 74, 74)
        )
        self.assertEqual(
            {p['case_id']: p['public_photo_count'] for p in profile['case_profiles']},
            {
                palette.OFFICIAL_CASE: 32,
                palette.AWAY_BACK_CASE: 32,
                palette.RACE_CASE: 2,
                watermark.OFFICIAL_CASE: 4,
                filename.OFFICIAL_CASE: 4
            }
        )
        self.assertEqual(len({p['directory'] for p in profile['case_profiles']}), 5)
        self.assertFalse(profile['large_24mp_generated'])
        self.assertEqual(len(result['manifest']['original_sha256']), 74)
        self.assertLess(len(set(result['manifest']['original_sha256'].values())), 74)
        rows = [r for r in result['summary']['results'] if r['id'] == 'UX-UND-018']
        self.assertEqual(len(rows), 3)
        self.assertTrue(all((r['original_count'] == 74 and r['status'] == 'PASS' for r in rows)))

    def test_all_profiles_preserve_generic1_10_20_two_dng_and_exact_union(self):
        profiles = [
            palette.OFFICIAL_CASE,
            palette.AWAY_BACK_CASE,
            palette.RACE_CASE,
            watermark.OFFICIAL_CASE,
            filename.OFFICIAL_CASE
        ]
        for other, count, union in [
            ('UX-RACE-005', 1, 77),
            ('UX-SEL-002', 10, 86),
            ('UX-IMP-009', 20, 96)
        ]:
            with self.subTest(other=other):
                result = self.run_cli([*profiles, other, 'UX-UND-018'])
                self.assertEqual(
                    (
                        result['code'],
                        result['base_calls'],
                        result['extra_calls'],
                        result['filename_calls']
                    ),
                    (0, 1, 3, 1)
                )
                self.assertEqual(result['base_options'], {'count': count, 'include_large': False})
                facts = result['summary']['fixture_profile']
                self.assertEqual(
                    (
                        facts['public_photo_count'],
                        facts['base_original_checksum_count'],
                        facts['extra_public_photo_count'],
                        facts['original_checksum_count']
                    ),
                    (count, count + 2, 74, union)
                )
                self.assertEqual(len(result['manifest']['original_sha256']), union)
                folders = dict(result['calls'])
                self.assertEqual(folders[other].name, 'public-fixtures-quality')
                for identifier in profiles:
                    self.assertEqual(folders[identifier].parts[-2:], ('case-fixtures', identifier))
                    self.assertFalse(folders[identifier].is_relative_to(folders[other]))
                integrity = next((r for r in result['summary']['results'] if r['id'] == 'UX-UND-018'))
                self.assertEqual(
                    (integrity['original_count'], integrity['status']),
                    (union, 'PASS')
                )

    def test_ineligible_filtered_unknown_unadapted_and_preflight_no_factory_or_child(self):
        variants = (
            {'native': False},
            {'override_row': {'scope': '0.6'}},
            {'extra_flags': ('--suite', 'blocking')},
            {'extra_flags': ('--require-build-provenance',), 'provenance': 'FAIL'},
            {'extra_flags': ('--variant', 'ignored')},
            {'extra_flags': ('--stage', 'ignored')}
        )
        for options in [dict(
            item,
            quality=quality
        ) for item in variants for quality in (False, True)] + [{'denied': True}]:
            with self.subTest(options=options):
                result = self.run_cli([filename.OFFICIAL_CASE], **options)
                self.assertNotEqual(result['code'], 0)
                self.assertEqual(
                    (
                        result['base_calls'],
                        result['extra_calls'],
                        result['filename_calls'],
                        len(result['calls'])
                    ),
                    (0, 0, 0, 0)
                )
                if result['summary']:
                    self.assertEqual(result['summary']['release_gate'], 'NOT_QUALIFIED')
                    self.assertFalse(any((r['status'] == 'PASS' for r in result['summary']['results'])))
        with patch(
            'sys.argv',
            ['runner', '--case', 'UX-VIS-999']
        ), patch(
            'sys.stderr',
            io.StringIO()
        ), patch.object(
            runner,
            'current_filename_fixture'
        ) as extra, patch.object(
            runner,
            'fixtures'
        ) as generic, patch.object(runner, 'execute_scenario') as child:
            with self.assertRaises(SystemExit) as failure:
                runner.main()
            self.assertEqual(failure.exception.code, 2)
            extra.assert_not_called()
            generic.assert_not_called()
            child.assert_not_called()

    def test_duplicate_request_once_and_all_source_mutations_fail_closed(self):
        result = self.run_cli([filename.OFFICIAL_CASE, filename.OFFICIAL_CASE])
        self.assertEqual(
            (result['code'], result['filename_calls'], len(result['calls'])),
            (0, 1, 1)
        )
        for mutation in ('bytes', 'remove', 'symlink'):
            with self.subTest(mutation=mutation):
                result = self.run_cli(
                    [filename.OFFICIAL_CASE],
                    mutate=mutation,
                    mutation_case=filename.OFFICIAL_CASE,
                    mutation_name='Short-control.png'
                )
                self.assertEqual(result['code'], 1)
                self.assertEqual(result['summary']['original_hashes'], 'FAIL')
                self.assertEqual(result['summary']['results'][0]['original_integrity'], 'FAIL')
                self.assertEqual(len(result['summary']['results'][0]['mutated_originals']), 1)

    def test_old_profiles_and_unrelated_quality_source_counts_unchanged(self):
        result = self.run_cli([
            palette.OFFICIAL_CASE,
            palette.AWAY_BACK_CASE,
            palette.RACE_CASE,
            watermark.OFFICIAL_CASE,
            'UX-UND-018'
        ])
        self.assertEqual((result['code'], result['filename_calls']), (0, 0))
        self.assertEqual(result['summary']['fixture_profile']['original_checksum_count'], 70)
        result = self.run_cli(['UX-SEL-002'])
        self.assertEqual(
            (result['code'], result['base_calls'], result['extra_calls'], result['filename_calls']),
            (0, 1, 0, 0)
        )
        self.assertEqual(result['base_options'], {'count': 10, 'include_large': False})
        self.assertEqual(result['summary']['fixture_profile']['original_checksum_count'], 12)

    def test_inventory_policy_exact_case_ci_and_narrow_new_artifacts(self):
        row = next((r for r in runner.INVENTORY if r['id'] == filename.OFFICIAL_CASE))
        self.assertEqual(
            (row['scope'], row['mode'], row['severity']),
            ('CURRENT', 'REGRESSION', 'P2')
        )
        self.assertIn(row['id'], runner.NATIVE)
        self.assertIsNone(policy.quality_skip_reason(row))
        for name in ('macos.yml', 'release.yml'):
            workflow = (runner.ROOT / '.github/workflows' / name).read_text()
            self.assertEqual(
                workflow.count('run_ux_suite.py --build build --quality-only --case UX-VIS-003 --runs 3'),
                1
            )
            self.assertIn('build/ux-export-current-filename-report/', workflow)
            self.assertIn('build/ux-artifacts/20*/**/vis003-approved-evidence.json', workflow)
            self.assertIn('build/ux-artifacts/20*/**/vis003-current-label-*.png', workflow)
            self.assertIn('build/ux-artifacts/20*/**/vis003-active-export-*.png', workflow)
            self.assertNotIn('run_vis003_approved_candidate.py', workflow)
            self.assertNotIn('build/ux-artifacts/20*/case-fixtures/', workflow)
            vis_lines = [line for line in workflow.splitlines() if 'vis003' in line or 'export-current-filename' in line]
            self.assertFalse(any(('sqlite' in line or '/exports/' in line or 'public-fixture' in line for line in vis_lines)))
        self.assertIn(
            'build/ux-artifacts/20*/**/catalog-copy.sqlite',
            (runner.ROOT / '.github/workflows/macos.yml').read_text()
        )
if __name__ == '__main__':
    unittest.main()
