"""RACE007 fixture/profile/CLI infrastructure; mocked children, no Qt or UI."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch
import zlib

import crop_done_fixtures as crop
import export_current_filename_fixtures as filename
import export_watermark_fixtures as watermark
import quality_policy as policy
import race007_fixtures as partial
import run_ux_suite as runner
import test_thumbnail_response_fixtures as response_tests
import thumbnail_response_fixtures as palette


class PartialThumbnailFixtureTests(unittest.TestCase):
    def run_cli(self, *args, **kwargs):
        with patch.object(runner, 'partial_render_fixture', wraps=partial.official_fixture) as factory:
            outcome = response_tests.ThumbnailFixtureTests.run_cli(self, *args, **kwargs)
            outcome['partial_calls'] = factory.call_count
            return outcome

    def test_exact_three_literal_rgb_sources_crc_pixels_sha_and_no_overwrite(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            folder = root / 'case-fixtures' / partial.OFFICIAL_CASE
            hashes, manifest = partial.official_fixture(folder, owned_root=root)
            self.assertEqual(sorted(Path(path).name for path in hashes),
                             ['Photo-0000.png', 'Photo-0001.png', 'Photo-0002.png'])
            self.assertEqual((manifest['case_id'], manifest['name']),
                             ('UX-RACE-007', 'thumbnail-partial-render-rgb3-v1'))
            self.assertEqual((manifest['public_photo_count'], manifest['original_checksum_count']), (3, 3))
            self.assertEqual((manifest['width'], manifest['height'], manifest['png_color_type']),
                             (256, 192, 'RGB'))
            self.assertFalse(manifest['large_24mp_generated'])
            self.assertEqual(manifest['literal_colors'], [[32, 2, 2], [2, 32, 2], [2, 2, 32]])
            for index, color in enumerate(((32, 2, 2), (2, 32, 2), (2, 2, 32))):
                path = folder / f'Photo-{index:04d}.png'
                data = path.read_bytes()
                self.assertEqual(hashes[str(path)], hashlib.sha256(data).hexdigest())
                self.assertEqual(data[:8], b'\x89PNG\r\n\x1a\n')
                offset, compressed, kinds = 8, b'', []
                while offset < len(data):
                    size = struct.unpack('>I', data[offset:offset + 4])[0]
                    kind = data[offset + 4:offset + 8]
                    payload = data[offset + 8:offset + 8 + size]
                    crc = struct.unpack('>I', data[offset + 8 + size:offset + 12 + size])[0]
                    self.assertEqual(zlib.crc32(kind + payload), crc)
                    kinds.append(kind)
                    if kind == b'IHDR':
                        self.assertEqual(struct.unpack('>IIBBBBB', payload),
                                         (256, 192, 8, 2, 0, 0, 0))
                    elif kind == b'IDAT':
                        compressed += payload
                    offset += size + 12
                self.assertEqual(offset, len(data))
                self.assertEqual(kinds, [b'IHDR', b'IDAT', b'IEND'])
                self.assertEqual(zlib.decompress(compressed), (b'\0' + bytes(color)*256)*192)
            self.assertEqual(len(set(hashes.values())), 3)
            before = {path.name: path.read_bytes() for path in folder.iterdir()}
            with self.assertRaises(ValueError):
                partial.official_fixture(folder, owned_root=root)
            self.assertEqual({path.name: path.read_bytes() for path in folder.iterdir()}, before)
            self.assertEqual(runner.changed_originals(hashes), [])

    def test_foreign_relative_alias_and_legacy_case_paths_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            for folder in (root / 'foreign', root / 'case-fixtures' / palette.RACE_CASE,
                           root / 'case-fixtures' / 'CANDIDATE-RACE007-PARTIAL-THUMBNAIL',
                           Path('case-fixtures') / partial.OFFICIAL_CASE):
                with self.subTest(folder=folder), self.assertRaises(ValueError):
                    partial.official_fixture(folder, owned_root=root)
            self.assertFalse((root / 'case-fixtures').exists())
            outside = root / 'outside'
            outside.mkdir()
            alias = root / 'alias'
            alias.symlink_to(root, target_is_directory=True)
            with self.assertRaises(ValueError):
                partial.official_fixture(alias / 'case-fixtures' / partial.OFFICIAL_CASE,
                                         owned_root=alias)
            parent = root / 'case-fixtures'
            parent.symlink_to(outside, target_is_directory=True)
            with self.assertRaises(ValueError):
                partial.official_fixture(parent / partial.OFFICIAL_CASE, owned_root=root)
            self.assertEqual(list(outside.iterdir()), [])

    def test_solo_three_mock_repeats_both_modes_only_three_profile_inputs(self):
        for quality in (False, True):
            with self.subTest(quality=quality):
                outcome = self.run_cli([partial.OFFICIAL_CASE], quality=quality, runs=3)
                self.assertEqual((outcome['code'], outcome['base_calls'], outcome['extra_calls'],
                                  outcome['partial_calls'], len(outcome['calls'])), (0, 0, 0, 1, 3))
                folders = {str(folder) for _, folder in outcome['calls']}
                self.assertEqual(len(folders), 1)
                self.assertTrue(all(folder.parts[-2:] == ('case-fixtures', 'UX-RACE-007')
                                    for _, folder in outcome['calls']))
                summary = outcome['summary']
                self.assertEqual((summary['inventory_domain'], summary['release_gate'],
                                  summary['execution_status']), ('specification', 'NOT_QUALIFIED', 'PASS'))
                profile = summary['fixture_profile']
                self.assertEqual((profile['base_original_checksum_count'], profile['extra_public_photo_count'],
                                  profile['extra_original_checksum_count'], profile['original_checksum_count']),
                                 (0, 3, 3, 3))
                self.assertEqual(profile, outcome['manifest']['fixture_profile'])
                self.assertEqual(len(outcome['manifest']['original_sha256']), 3)
                self.assertFalse(profile['large_24mp_generated'])

    def test_mixed_generic_race002_and_other_profiles_preserve_base_counts(self):
        for generic, count in (('UX-RACE-005', 1), ('UX-SEL-002', 10), ('UX-IMP-009', 20)):
            with self.subTest(generic=generic):
                outcome = self.run_cli([partial.OFFICIAL_CASE, palette.RACE_CASE, generic])
                self.assertEqual(outcome['code'], 0)
                self.assertEqual((outcome['base_calls'], outcome['extra_calls'], outcome['partial_calls']),
                                 (1, 1, 1))
                self.assertEqual(outcome['base_options'], {'count': count, 'include_large': False})
                profile = outcome['summary']['fixture_profile']
                self.assertEqual((profile['base_original_checksum_count'], profile['extra_original_checksum_count'],
                                  profile['original_checksum_count']), (count + 2, 5, count + 7))
                folders = dict(outcome['calls'])
                self.assertEqual(folders[generic].name, 'public-fixtures-quality')
                self.assertEqual(folders[partial.OFFICIAL_CASE].name, 'UX-RACE-007')
                self.assertEqual(folders[palette.RACE_CASE].name, 'UX-RACE-002')
                self.assertEqual(len(set(folders.values())), 3)

    def test_all_seven_profiles_union78_with_generic90_100_and_global78(self):
        identifiers = [palette.OFFICIAL_CASE, palette.AWAY_BACK_CASE, palette.RACE_CASE,
                       watermark.OFFICIAL_CASE, crop.OFFICIAL_CASE, filename.OFFICIAL_CASE,
                       partial.OFFICIAL_CASE]
        for generic, total in ((None, 78), ('UX-SEL-002', 90), ('UX-IMP-009', 100)):
            with self.subTest(generic=generic), patch.dict(os.environ, {'GITHUB_ACTIONS': ''}):
                outcome = self.run_cli(identifiers + ([generic] if generic else []) + ['UX-UND-018'])
                self.assertEqual(outcome['code'], 0)
                profile = outcome['summary']['fixture_profile']
                self.assertEqual((profile['extra_original_checksum_count'], profile['original_checksum_count']),
                                 (78, total))
                self.assertEqual(len(profile['case_profiles']), 7)
                self.assertEqual({p['case_id']:p['original_checksum_count'] for p in profile['case_profiles']},
                                 {'UX-LIB-004':32, 'UX-LIB-005':32, 'UX-RACE-002':2,
                                  'UX-EXP-028':4, 'UX-CRP-010':1, 'UX-VIS-003':4, 'UX-RACE-007':3})
                self.assertEqual(len({row['directory'] for row in profile['case_profiles']}), 7)
                self.assertEqual(len(outcome['manifest']['original_sha256']), total)
                self.assertFalse(profile['large_24mp_generated'])
                integrity, = [r for r in outcome['summary']['results'] if r['id'] == 'UX-UND-018']
                self.assertEqual((integrity['original_count'], integrity['status']), (total, 'PASS'))
                self.assertEqual(outcome['base_options'], None if generic is None else
                                 {'count': 20 if generic == 'UX-IMP-009' else 10, 'include_large': False})

    def test_known_software_skips_only_native_crop_keeps_partial_render_and_union77(self):
        identifiers = [palette.OFFICIAL_CASE, palette.AWAY_BACK_CASE, palette.RACE_CASE,
                       watermark.OFFICIAL_CASE, crop.OFFICIAL_CASE, filename.OFFICIAL_CASE,
                       partial.OFFICIAL_CASE]
        for generic, total in ((None, 77), ('UX-SEL-002', 89), ('UX-IMP-009', 99)):
            with self.subTest(generic=generic), patch.dict(os.environ, {'GITHUB_ACTIONS': 'true'}):
                outcome = self.run_cli(identifiers + ([generic] if generic else []) + ['UX-UND-018'])
                self.assertEqual(outcome['code'], 1)
                summary = outcome['summary']
                self.assertEqual((summary['execution_status'], summary['release_gate']),
                                 ('INCOMPLETE', 'NOT_QUALIFIED'))
                self.assertEqual(summary['unsupported_backend'], [crop.OFFICIAL_CASE])
                self.assertEqual(summary['fixture_profile']['original_checksum_count'], total)
                self.assertEqual(summary['fixture_profile']['extra_original_checksum_count'], 77)
                self.assertEqual(outcome['partial_calls'], 1)
                self.assertIn(partial.OFFICIAL_CASE, {identifier for identifier, _ in outcome['calls']})
                crop_row, = [r for r in summary['results'] if r['id'] == crop.OFFICIAL_CASE]
                self.assertEqual(crop_row['status'], 'SKIPPED')
                self.assertNotIn('child_pid', crop_row)

    def test_ineligible_requests_zero_factories_children_no_hidden_default_load(self):
        options_list = ({'native': False}, {'override_row': {'scope': '0.6'}},
                        {'extra_flags': ('--suite', 'blocking')},
                        {'extra_flags': ('--require-build-provenance',), 'provenance': 'FAIL'})
        for options in [dict(option, quality=quality) for option in options_list
                        for quality in (False, True)] + [{'denied': True}]:
            with self.subTest(options=options):
                outcome = self.run_cli([partial.OFFICIAL_CASE], **options)
                self.assertNotEqual(outcome['code'], 0)
                self.assertEqual((outcome['base_calls'], outcome['extra_calls'],
                                  outcome['partial_calls'], len(outcome['calls'])), (0, 0, 0, 0))
                if outcome['summary'] is not None:
                    self.assertEqual(outcome['summary']['execution_status'], 'INCOMPLETE')
                    self.assertEqual(outcome['summary']['release_gate'], 'NOT_QUALIFIED')
                    self.assertFalse(any(r['status'] == 'PASS' for r in outcome['summary']['results']))
        for quality in (False, True):
            with self.subTest(mixed=True, quality=quality), patch.dict(os.environ, {'GITHUB_ACTIONS': 'true'}):
                outcome = self.run_cli([partial.OFFICIAL_CASE, 'UX-MAC-001'], quality=quality, native=False)
                self.assertNotEqual(outcome['code'], 0)
                self.assertEqual((outcome['base_calls'], outcome['extra_calls'],
                                  outcome['partial_calls'], len(outcome['calls'])), (0, 0, 0, 0))

    def test_mutation_remove_and_same_bytes_symlink_are_p0_fail_and_global_checks_three(self):
        for mutation in ('bytes', 'remove', 'symlink'):
            with self.subTest(mutation=mutation):
                outcome = self.run_cli([partial.OFFICIAL_CASE], mutate=mutation,
                                       mutation_case=partial.OFFICIAL_CASE)
                self.assertEqual(outcome['code'], 1)
                self.assertEqual(outcome['summary']['original_hashes'], 'FAIL')
                row = outcome['summary']['results'][0]
                self.assertEqual(row['original_integrity'], 'FAIL')
                self.assertEqual(len(row['mutated_originals']), 1)
        outcome = self.run_cli([partial.OFFICIAL_CASE, 'UX-UND-018'], runs=3)
        self.assertEqual(outcome['code'], 0)
        integrity = [r for r in outcome['summary']['results'] if r['id'] == 'UX-UND-018']
        self.assertEqual(len(integrity), 3)
        self.assertTrue(all(r['status'] == 'PASS' and r['original_count'] == 3 for r in integrity))

    def test_legacy_supplemental_selection_never_calls_new_official_factory(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            build = root / 'build'
            build.mkdir()
            snapshot = {'source_tree_sha256': 'legacy-source', 'test_executable_sha256': 'legacy-binary'}
            identifier = 'CANDIDATE-RACE007-PARTIAL-THUMBNAIL'
            row = {'id': identifier, 'scope': 'CURRENT', 'mode': 'REGRESSION',
                   'severity': 'P1', 'scenario': 'Existing supplemental routing mock'}
            def legacy(folder):
                folder.mkdir(parents=True)
                hashes = {}
                for index in range(3):
                    path = folder / f'Photo-{index:04d}.png'
                    path.write_bytes(b'unchanged legacy mock source ' + str(index).encode())
                    hashes[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
                return hashes
            def execute(case, destination, arguments, fixture_folder):
                destination.mkdir(parents=True)
                return {**case, 'status': 'PASS', 'child_pid': 123, 'artifacts': str(destination)}
            argv = ['legacy', '--build', str(build), '--runs', '3']
            with (patch.object(runner, 'source_phase', return_value='0.5'),
                  patch.object(runner, 'fingerprint', return_value=snapshot),
                  patch.object(runner, 'build_provenance', return_value={'status': 'PASS'}),
                  patch.object(runner, 'fixtures') as generic,
                  patch.object(runner, 'partial_render_fixture') as new,
                  patch.object(runner, 'execute_scenario', side_effect=execute) as children,
                  patch.object(runner, 'write_reports') as reports,
                  patch.object(runner.fcntl, 'flock'),
                  patch.object(runner.tempfile, 'gettempdir', return_value=str(root)),
                  patch('sys.argv', argv), patch('sys.stdout', io.StringIO())):
                code = runner.main(inventory=[row], executable_ids={identifier},
                                   fixture_factory=legacy, domain='supplemental')
            self.assertEqual((code, generic.call_count, new.call_count, children.call_count), (0, 0, 0, 3))
            summary = reports.call_args.args[0]
            self.assertEqual((summary['inventory_domain'], summary['release_gate']),
                             ('supplemental', 'NOT_QUALIFIED'))
            self.assertEqual(summary['fixture_profile']['original_checksum_count'], 3)
            self.assertEqual(summary['fixture_profile']['case_profiles'], [])

    def test_policy_capabilities_original_spec_and_focused_ci_artifacts(self):
        row = next(row for row in runner.INVENTORY if row['id'] == partial.OFFICIAL_CASE)
        self.assertEqual((row['scope'], row['mode'], row['severity']), ('CURRENT', 'REGRESSION', 'P1'))
        self.assertEqual(row['scenario'], 'batch commit while thumbnail workers render old revisions.')
        self.assertIn(row['id'], runner.NATIVE)
        self.assertIn(row['id'], runner.CASE_PROFILE_IDS)
        self.assertNotIn(row['id'], runner.NATIVE_ONLY_CONTRACTS)
        self.assertIsNone(policy.quality_skip_reason(row))
        self.assertFalse(runner.row_selected(row, argparse.Namespace(
            case=[row['id']], suite='blocking', phase=None)))
        self.assertEqual(policy.quality_fixture_count([{'id': row['id']}]), 3)
        self.assertEqual(policy.quality_fixture_count([{'id': row['id']}, {'id': 'UX-SEL-002'}]), 10)
        for capability in ('library', 'develop', 'presets', 'batch'):
            self.assertTrue(runner.MANIFEST['capabilities'][capability])
        for name in ('macos.yml', 'release.yml'):
            workflow = (runner.ROOT / '.github/workflows' / name).read_text()
            self.assertEqual(workflow.count('--case UX-RACE-007 --runs 3'), 1)
            self.assertIn('run_ux_suite.py --build build --quality-only --case UX-RACE-007 --runs 3 --require-build-provenance --report build/ux-thumbnail-partial-render-report', workflow)
            for path in ('build/ux-thumbnail-partial-render-report/',
                         'build/ux-artifacts/20*/**/race007-*.json',
                         'build/ux-artifacts/20*/**/race007-node-*.png'):
                self.assertIn(path, workflow)
            self.assertNotIn('build/ux-artifacts/20*/case-fixtures/', workflow)
            if name == 'macos.yml':
                self.assertIn('build/ux-artifacts/20*/**/catalog-copy.sqlite', workflow)


if __name__ == '__main__':
    unittest.main()
