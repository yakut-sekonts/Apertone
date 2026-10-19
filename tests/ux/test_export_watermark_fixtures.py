"""Official EXP028 profile/child environment invariants; no Qt/native process."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import stat
import struct
import tempfile
import unittest
from unittest.mock import patch
import uuid
import zlib

import export_watermark_fixtures as watermark
import quality_policy as policy
import run_ux_suite as runner
import test_thumbnail_response_fixtures as response_tests
import thumbnail_response_fixtures as palette


def decode_rgb_png(data):
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise AssertionError('Invalid PNG signature')
    offset, compressed, header, kinds = 8, b'', None, []
    while offset < len(data):
        size = struct.unpack('>I', data[offset:offset + 4])[0]
        kind, payload = data[offset + 4:offset + 8], data[offset + 8:offset + 8 + size]
        if zlib.crc32(kind + payload) != struct.unpack('>I', data[offset + 8 + size:offset + 12 + size])[0]:
            raise AssertionError('Invalid PNG CRC')
        kinds.append(kind)
        if kind == b'IHDR':
            header = struct.unpack('>IIBBBBB', payload)
        if kind == b'IDAT':
            compressed += payload
        offset += size + 12
    if offset != len(data) or kinds != [b'IHDR', b'IDAT', b'IEND']:
        raise AssertionError('PNG chunk inventory differs')
    return header, zlib.decompress(compressed)


class WatermarkFixtureTests(unittest.TestCase):
    def run_cli(self, *args, **kwargs):
        with patch.object(runner, 'watermark_fixture', wraps=watermark.official_fixture) as factory:
            result = response_tests.ThumbnailFixtureTests.run_cli(self, *args, **kwargs)
            result['watermark_calls'] = factory.call_count
            return result

    def test_exact_four_new_rgb_sources_crc_pixels_and_manifest(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            folder = root / 'case-fixtures' / watermark.OFFICIAL_CASE
            hashes, manifest = watermark.official_fixture(folder, owned_root=root)
            expected_names = (*watermark.PHOTO_NAMES, watermark.WATERMARK_NAME)
            self.assertEqual({path.name for path in folder.iterdir()}, set(expected_names))
            self.assertEqual((len(hashes), manifest['public_photo_count'],
                              manifest['original_checksum_count']), (4, 4, 4))
            self.assertEqual((manifest['importable_photo_count'], manifest['watermark_fixture_count']), (3, 1))
            self.assertEqual(manifest['photo_dimensions'], {'width': 64, 'height': 48})
            self.assertEqual(manifest['watermark_dimensions'], {'width': 32, 'height': 32})
            self.assertFalse(manifest['large_24mp_generated'])
            self.assertEqual(manifest['png_color_type'], 'RGB')
            for variant, name in enumerate(expected_names):
                path = folder / name
                data = path.read_bytes()
                self.assertEqual(hashes[str(path)], hashlib.sha256(data).hexdigest())
                header, pixels = decode_rgb_png(data)
                width, height = (32, 32) if variant == 3 else (64, 48)
                self.assertEqual(header, (width, height, 8, 2, 0, 0, 0))
                expected = b''
                for y in range(height):
                    row = b''
                    for x in range(width):
                        color = ((255, 0, 0) if variant == 3 else
                                 (35 + 65 * variant + x % 13, 55 + y % 17, 70 + (x + y) % 11))
                        row += bytes(color)
                    expected += b'\0' + row
                self.assertEqual(pixels, expected)
            self.assertEqual(runner.changed_originals(hashes), [])
            before = {name: path.read_bytes() for name, path in
                      ((name, folder / name) for name in expected_names)}
            with self.assertRaises(ValueError):
                watermark.official_fixture(folder, owned_root=root)
            self.assertEqual({name: (folder / name).read_bytes() for name in expected_names}, before)

    def test_profile_rejects_foreign_case_root_alias_and_preexisting_directory(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            outside = root / 'outside'
            outside.mkdir()
            for folder in (outside, root / 'case-fixtures' / palette.OFFICIAL_CASE,
                           root / 'case-fixtures' / palette.RACE_CASE):
                with self.subTest(folder=folder), self.assertRaises(ValueError):
                    watermark.official_fixture(folder, owned_root=root)
            alias = root / 'alias'
            alias.symlink_to(root, target_is_directory=True)
            with self.assertRaises(ValueError):
                watermark.official_fixture(alias / 'case-fixtures' / watermark.OFFICIAL_CASE,
                                           owned_root=alias)
            (root / 'case-fixtures').symlink_to(outside, target_is_directory=True)
            with self.assertRaises(ValueError):
                watermark.official_fixture(root / 'case-fixtures' / watermark.OFFICIAL_CASE,
                                           owned_root=root)
            self.assertEqual(list(outside.iterdir()), [])

    def test_solo_official_reuses_four_inputs_without_generic_or_bulk_setup(self):
        for quality in (False, True):
            with self.subTest(quality=quality):
                result = self.run_cli([watermark.OFFICIAL_CASE], quality=quality, runs=3)
                self.assertEqual((result['code'], result['base_calls'], result['extra_calls'],
                                  result['watermark_calls']), (0, 0, 0, 1))
                self.assertEqual(len(result['calls']), 3)
                self.assertEqual(len({str(folder) for _, folder in result['calls']}), 1)
                self.assertTrue(all(folder.parts[-2:] == ('case-fixtures', watermark.OFFICIAL_CASE)
                                    for _, folder in result['calls']))
                summary = result['summary']
                profile = summary['fixture_profile']
                self.assertEqual((profile['public_photo_count'], profile['base_original_checksum_count'],
                                  profile['extra_public_photo_count'], profile['original_checksum_count']),
                                 (0, 0, 4, 4))
                self.assertFalse(profile['large_24mp_generated'])
                self.assertEqual(profile, result['manifest']['fixture_profile'])
                self.assertEqual(len(result['manifest']['original_sha256']), 4)
                self.assertEqual((summary['inventory_domain'], summary['release_gate']),
                                 ('specification', 'NOT_QUALIFIED'))

    def test_combined_profiles_and_generic_roots_preserve_old_counts_and_union(self):
        profiles = [palette.OFFICIAL_CASE, palette.RACE_CASE, watermark.OFFICIAL_CASE]
        for other, count in [('UX-RACE-005', 1), ('UX-SEL-002', 10), ('UX-IMP-009', 20)]:
            with self.subTest(other=other):
                result = self.run_cli([*profiles, other])
                self.assertEqual((result['code'], result['base_calls'], result['extra_calls'],
                                  result['watermark_calls']), (0, 1, 2, 1))
                self.assertEqual(result['base_options'], {'count': count, 'include_large': False})
                folders = dict(result['calls'])
                self.assertEqual(folders[other].name, 'public-fixtures-quality')
                self.assertEqual(len({str(folder) for folder in folders.values()}), 4)
                for identifier in profiles:
                    self.assertEqual(folders[identifier].parts[-2:], ('case-fixtures', identifier))
                    self.assertFalse(folders[identifier].is_relative_to(folders[other]))
                profile = result['summary']['fixture_profile']
                self.assertEqual((profile['public_photo_count'], profile['base_original_checksum_count'],
                                  profile['extra_public_photo_count'], profile['original_checksum_count']),
                                 (count, count + 2, 38, count + 40))
                self.assertEqual({entry['case_id']: entry['original_checksum_count']
                                  for entry in profile['case_profiles']},
                                 {palette.OFFICIAL_CASE: 32, palette.RACE_CASE: 2, watermark.OFFICIAL_CASE: 4})
                self.assertEqual(len(result['manifest']['original_sha256']), count + 40)

    def test_union_integrity_duplicates_and_original_mutations(self):
        result = self.run_cli([watermark.OFFICIAL_CASE, watermark.OFFICIAL_CASE])
        self.assertEqual((result['code'], result['watermark_calls'], len(result['calls'])), (0, 1, 1))
        for mutation in ('bytes', 'remove', 'symlink'):
            with self.subTest(mutation=mutation):
                result = self.run_cli([watermark.OFFICIAL_CASE], mutate=mutation,
                                      mutation_case=watermark.OFFICIAL_CASE, mutation_name='A-owned.png')
                self.assertEqual(result['code'], 1)
                self.assertEqual(result['summary']['original_hashes'], 'FAIL')
                self.assertEqual(result['summary']['results'][0]['original_integrity'], 'FAIL')
        result = self.run_cli([palette.OFFICIAL_CASE, palette.RACE_CASE, watermark.OFFICIAL_CASE,
                               'UX-UND-018'], runs=3)
        rows = [row for row in result['summary']['results'] if row['id'] == 'UX-UND-018']
        self.assertEqual(len(rows), 3)
        self.assertTrue(all(row['original_count'] == 38 and row['status'] == 'PASS' for row in rows))

    def test_ineligible_requests_and_preflight_flags_generate_no_extra_or_base(self):
        options = ({'native': False}, {'override_row': {'scope': '0.6'}},
                   {'extra_flags': ('--suite', 'blocking')},
                   {'extra_flags': ('--require-build-provenance',), 'provenance': 'FAIL'},
                   {'extra_flags': ('--variant', 'ignored')},
                   {'extra_flags': ('--stage', 'ignored')})
        for item in [dict(item, quality=quality) for item in options for quality in (False, True)] + [{'denied': True}]:
            with self.subTest(item=item):
                result = self.run_cli([watermark.OFFICIAL_CASE], **item)
                self.assertNotEqual(result['code'], 0)
                self.assertEqual((result['base_calls'], result['extra_calls'], result['watermark_calls']), (0, 0, 0))
                self.assertFalse(result['calls'])
                if result['summary']:
                    self.assertEqual(result['summary']['release_gate'], 'NOT_QUALIFIED')
                    self.assertFalse(any(row['status'] == 'PASS' for row in result['summary']['results']))

    def prepared(self, root, repeat='repeat-1'):
        folder = root / 'case-fixtures' / watermark.OFFICIAL_CASE
        watermark.official_fixture(folder, owned_root=root)
        directory = root / watermark.OFFICIAL_CASE / repeat
        directory.mkdir(parents=True)
        return directory, folder

    def test_child_environment_is_a_copy_with_exact_fresh_owned_root_marker_and_mode(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            directory, folder = self.prepared(root)
            parent = {'TMPDIR': '/previous/temp', 'OTHER': 'kept',
                      'LUMA_UX_EXP028_CASE_ROOT': 'poison', 'LUMA_UX_EXP028_TMP_ROOT': 'poison'}
            before = dict(parent)
            global_before = dict(os.environ)
            result = watermark.child_environment(parent, directory, folder, case_id=watermark.OFFICIAL_CASE)
            self.assertEqual(parent, before)
            self.assertEqual(dict(os.environ), global_before)
            self.assertIsNot(result, parent)
            self.assertEqual(result['OTHER'], 'kept')
            temporary = Path(str(directory) + '-owned-runtime-temp')
            self.assertEqual(result['TMPDIR'], str(temporary))
            self.assertEqual(result['LUMA_UX_EXP028_CASE_ROOT'], str(directory))
            self.assertEqual(result['LUMA_UX_EXP028_TMP_ROOT'], str(temporary))
            self.assertEqual(stat.S_IMODE(temporary.stat().st_mode), 0o700)
            marker = json.loads((temporary / 'wrapper-owner.json').read_text())
            self.assertEqual((marker['case'], marker['case_root'], marker['temp_root'], marker['wrapper_pid']),
                             (watermark.OFFICIAL_CASE, str(directory), str(temporary), os.getpid()))
            self.assertEqual(str(uuid.UUID(marker['nonce'])), marker['nonce'])
            with self.assertRaises(ValueError):
                watermark.child_environment(parent, directory, folder, case_id=watermark.OFFICIAL_CASE)
            directory2 = directory.with_name('repeat-2')
            directory2.mkdir()
            other = watermark.child_environment(parent, directory2, folder, case_id=watermark.OFFICIAL_CASE)
            marker2 = json.loads((Path(other['TMPDIR']) / 'wrapper-owner.json').read_text())
            self.assertNotEqual(marker['nonce'], marker2['nonce'])
            self.assertNotEqual(result['TMPDIR'], other['TMPDIR'])

    def test_child_environment_rejects_unrelated_candidate_foreign_alias_and_wrong_repeat(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            directory, folder = self.prepared(root)
            foreign = root / 'foreign' / watermark.OFFICIAL_CASE / 'repeat-1'
            foreign.mkdir(parents=True)
            alias = root / 'alias'
            alias.symlink_to(root, target_is_directory=True)
            bad_repeat = directory.with_name('repeat-01')
            bad_repeat.mkdir()
            for case_id, candidate, fixture in [
                    ('UX-SEL-002', directory, folder),
                    ('CANDIDATE-EXP028-WATERMARK-SNAPSHOT-MISSING', directory, folder),
                    (watermark.OFFICIAL_CASE, foreign, folder),
                    (watermark.OFFICIAL_CASE, alias / watermark.OFFICIAL_CASE / 'repeat-1', folder),
                    (watermark.OFFICIAL_CASE, directory, alias / 'case-fixtures' / watermark.OFFICIAL_CASE),
                    (watermark.OFFICIAL_CASE, bad_repeat, folder),
                    (watermark.OFFICIAL_CASE, directory, root / 'case-fixtures' / palette.RACE_CASE)]:
                with self.subTest(candidate=candidate, case_id=case_id), self.assertRaises(ValueError):
                    watermark.child_environment({}, candidate, fixture, case_id=case_id)
            self.assertFalse(list(root.rglob('wrapper-owner.json')))

    def test_child_environment_never_reuses_file_directory_or_symlink_temp_sibling(self):
        for kind in ('file', 'directory', 'symlink', 'broken_symlink'):
            with self.subTest(kind=kind), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary).resolve()
                directory, folder = self.prepared(root)
                target = Path(str(directory) + '-owned-runtime-temp')
                outside = root / 'do-not-touch'
                outside.mkdir()
                if kind == 'file':
                    target.write_bytes(b'existing control')
                elif kind == 'directory':
                    target.mkdir()
                else:
                    target.symlink_to(outside if kind == 'symlink' else root / 'missing', target_is_directory=True)
                with self.assertRaises(ValueError):
                    watermark.child_environment({}, directory, folder, case_id=watermark.OFFICIAL_CASE)
                self.assertEqual(list(outside.iterdir()), [])
                self.assertFalse(list(root.rglob('wrapper-owner.json')))

    def test_execute_passes_owned_environment_to_popen_only_for_official_id(self):
        for identifier in (watermark.OFFICIAL_CASE, 'UX-SEL-002',
                           'CANDIDATE-EXP028-WATERMARK-SNAPSHOT-MISSING'):
            with self.subTest(identifier=identifier), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary).resolve()
                folder = root / 'case-fixtures' / watermark.OFFICIAL_CASE
                watermark.official_fixture(folder, owned_root=root)
                directory = root / identifier / 'repeat-1'
                captured = []
                class Process:
                    pid = 123
                    returncode = 0
                    def poll(self):
                        return 0
                def popen(command, **kwargs):
                    captured.append(kwargs['env'])
                    (directory / 'result.json').write_text(json.dumps({'id': identifier, 'status': 'PASS'}))
                    return Process()
                environment = {'TMPDIR': '/unchanged-parent-temp', 'GITHUB_ACTIONS': 'true'}
                with (patch.dict(os.environ, environment, clear=True),
                      patch.object(runner.subprocess, 'Popen', side_effect=popen),
                      patch.object(runner, 'watermark_child_environment', wraps=watermark.child_environment) as helper):
                    before = dict(os.environ)
                    result = runner.execute({'id': identifier, 'scope': 'CURRENT',
                                             'mode': 'REGRESSION', 'severity': 'P1',
                                             'scenario': 'Mock-only launch environment'}, directory,
                                            argparse.Namespace(build=root / 'build', timeout=10), folder)
                    self.assertEqual(dict(os.environ), before)
                    self.assertEqual(result['status'], 'PASS')
                    self.assertEqual(helper.call_count, int(identifier == watermark.OFFICIAL_CASE))
                self.assertEqual(len(captured), 1)
                self.assertEqual(captured[0]['QT_QPA_PLATFORM'], 'offscreen')
                self.assertEqual(captured[0]['QT_QUICK_BACKEND'], 'software')
                self.assertEqual(captured[0]['TMPDIR'],
                                 str(directory) + '-owned-runtime-temp' if identifier == watermark.OFFICIAL_CASE
                                 else environment['TMPDIR'])
                if identifier != watermark.OFFICIAL_CASE:
                    self.assertNotIn('LUMA_UX_EXP028_CASE_ROOT', captured[0])
                    self.assertNotIn('LUMA_UX_EXP028_TMP_ROOT', captured[0])

    def test_official_inventory_policy_and_ci_keep_narrow_domain_and_artifacts(self):
        row = next(row for row in runner.INVENTORY if row['id'] == watermark.OFFICIAL_CASE)
        self.assertEqual((row['scope'], row['mode'], row['severity']), ('CURRENT', 'REGRESSION', 'P1'))
        self.assertIn(row['id'], runner.NATIVE)
        self.assertIsNone(policy.quality_skip_reason(row))
        for name in ('macos.yml', 'release.yml'):
            workflow = (runner.ROOT / '.github/workflows' / name).read_text()
            self.assertIn('run_ux_suite.py --build build --quality-only --case UX-EXP-028 --runs 3', workflow)
            self.assertIn('build/ux-watermark-snapshot-report/', workflow)
            self.assertIn('build/ux-artifacts/20*/**/exp028-evidence.json', workflow)
            self.assertIn('build/ux-artifacts/20*/fixture-manifest.json', workflow)
            for forbidden in ('case-fixtures/', 'owned-runtime-temp/', 'run_exp028_candidate.py'):
                self.assertNotIn(forbidden, workflow)
            exp_paths = {line.strip() for line in workflow.splitlines()
                         if line.strip().startswith('build/') and
                         ('exp028' in line or 'watermark-snapshot' in line)}
            self.assertEqual(exp_paths, {'build/ux-watermark-snapshot-report/',
                                         'build/ux-artifacts/20*/**/exp028-evidence.json'})


if __name__ == '__main__':
    unittest.main()
