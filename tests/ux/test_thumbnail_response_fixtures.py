"""Official LIB004 routing/accounting/safety; no native child, UI or large image."""
import hashlib
import io
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch
import zlib

import run_ux_suite as runner
import thumbnail_response_fixtures as palette
import quality_policy as policy


class ThumbnailFixtureTests(unittest.TestCase):
    def test_exact_fresh_owned_rgb_profile_and_no_overwrite(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            folder = root / 'case-fixtures' / palette.OFFICIAL_CASE
            hashes, manifest = palette.official_fixture(folder, owned_root=root)
            self.assertEqual(len(hashes), 32)
            self.assertEqual(set(path.name for path in folder.iterdir()),
                             {f'Photo-{index:04}.png' for index in range(32)})
            self.assertEqual(manifest['public_photo_count'], 32)
            self.assertEqual(manifest['original_checksum_count'], 32)
            self.assertEqual(manifest['png_color_type'], 'RGB')
            self.assertFalse(manifest['large_24mp_generated'])
            colors = set()
            for name, digest in hashes.items():
                data = Path(name).read_bytes()
                self.assertEqual(hashlib.sha256(data).hexdigest(), digest)
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
                pixels = zlib.decompress(compressed)
                color = pixels[1:4]
                self.assertEqual(pixels, (b'\0' + color * 256) * 192)
                colors.add(tuple(color))
            self.assertEqual(len(colors), 32)
            self.assertGreaterEqual(min(max(abs(a - b) for a, b in zip(left, right))
                                        for left in colors for right in colors if left != right), 70)
            with self.assertRaises(ValueError):
                palette.official_fixture(folder, owned_root=root)
            self.assertEqual(runner.changed_originals(hashes), [])

    def test_profile_rejects_existing_symlink_and_foreign_directories(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            folder = root / 'case-fixtures' / palette.OFFICIAL_CASE
            folder.parent.mkdir()
            folder.mkdir()
            with self.assertRaises(ValueError):
                palette.official_fixture(folder, owned_root=root)
            folder.rmdir()
            outside = root / 'foreign'
            outside.mkdir()
            folder.symlink_to(outside, target_is_directory=True)
            with self.assertRaises(ValueError):
                palette.official_fixture(folder, owned_root=root)
            self.assertEqual(list(outside.iterdir()), [])
            folder.unlink()
            with self.assertRaises(ValueError):
                palette.official_fixture(outside, owned_root=root)
            folder.parent.rmdir()
            folder.parent.symlink_to(outside, target_is_directory=True)
            with self.assertRaises(ValueError):
                palette.official_fixture(folder, owned_root=root)
            self.assertEqual(list(outside.iterdir()), [])

    def test_sha_union_rejects_duplicate_outside_symlink_and_wrong_digest(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            owned = root / 'owned'
            owned.mkdir()
            source = owned / 'original.png'
            source.write_bytes(b'original')
            digest = hashlib.sha256(source.read_bytes()).hexdigest()
            hashes = {str(source): digest}
            self.assertEqual(runner.merge_fixture_hashes([(owned, hashes)]), hashes)
            invalid = [([(owned, hashes), (owned, hashes)]),
                       ([(owned, {str(root / 'outside.png'): digest})]),
                       ([(owned, {str(source): '0' * 64})])]
            for profiles in invalid:
                with self.subTest(profiles=profiles), self.assertRaises(ValueError):
                    runner.merge_fixture_hashes(profiles)
            link = owned / 'link.png'
            link.symlink_to(source)
            with self.assertRaises(ValueError):
                runner.merge_fixture_hashes([(owned, {str(link): digest})])
            self.assertEqual(runner.changed_originals({str(link): digest}), [str(link)])
            source.unlink()
            source.symlink_to(link)
            self.assertEqual(runner.changed_originals(hashes), [str(source)])

    def run_cli(self, identifiers, *, runs=1, quality=True, native=True,
                extra_flags=(), override_row=None, denied=False, provenance='PASS', mutate=False,
                mutation_case=palette.OFFICIAL_CASE):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            build = root / 'build'
            build.mkdir()
            rows = [dict(next(row for row in runner.INVENTORY if row['id'] == identifier))
                    for identifier in dict.fromkeys(identifiers)]
            if override_row:
                rows[0].update(override_row)
            selected_native = {row['id'] for row in rows if row['id'] not in runner.GLOBAL} if native else set()
            snapshot = {'source_tree_sha256': 'fixture-source', 'test_executable_sha256': 'fixture-binary'}
            calls = []

            def generic(folder, *, count, include_large):
                self.assertFalse(include_large)
                folder.mkdir(parents=True)
                for index in range(count):
                    (folder / f'Photo-{index:04}.png').write_bytes(b'unchanged generic ' + str(index).encode())
                (folder / 'stress').mkdir()
                for name in ('Lens.DNG', 'Building.DNG'):
                    (folder / 'stress' / name).write_bytes(b'unchanged small ' + name.encode())
                return {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                        for path in folder.rglob('*') if path.is_file()}

            def execute(row, destination, arguments, fixture_folder):
                destination.mkdir(parents=True)
                calls.append((row['id'], fixture_folder))
                if mutate and row['id'] == mutation_case:
                    source = fixture_folder / 'Photo-0000.png'
                    if mutate == 'remove':
                        source.unlink()
                    elif mutate == 'symlink':
                        replacement = destination / 'same-bytes.png'
                        replacement.write_bytes(source.read_bytes())
                        source.unlink()
                        source.symlink_to(replacement)
                    else:
                        source.write_bytes(b'mutated original')
                return {**row, 'status': 'PASS', 'child_pid': 123, 'artifacts': str(destination)}

            argv = ['runner', '--build', str(build), '--runs', str(runs)]
            if quality:
                argv.append('--quality-only')
            for identifier in identifiers:
                argv += ['--case', identifier]
            argv += list(extra_flags)
            stderr = io.StringIO()
            original_skip = policy.quality_skip_reason
            def skip(row):
                return ('deferred_load', 'test-only denied eligibility') if denied else original_skip(row)

            with (patch.object(runner, 'INVENTORY', rows),
                  patch.object(runner, 'NATIVE', selected_native),
                  patch.object(runner, 'source_phase', return_value='0.5'),
                  patch.object(runner, 'fingerprint', return_value=snapshot),
                  patch.object(runner, 'build_provenance', return_value={'status': provenance, 'path': 'proof', 'reason': 'test'}),
                  patch.object(runner, 'fixtures', side_effect=generic) as base_factory,
                  patch.object(runner, 'thumbnail_fixture', wraps=palette.official_fixture) as extra_factory,
                  patch.object(runner, 'quality_skip_reason', side_effect=skip),
                  patch.object(runner, 'execute_scenario', side_effect=execute),
                  patch.object(runner, 'write_reports') as reports,
                  patch.object(runner.fcntl, 'flock'),
                  patch.object(runner.tempfile, 'gettempdir', return_value=str(root)),
                  patch('sys.argv', argv), patch('sys.stderr', stderr), patch('builtins.print')):
                try:
                    code = runner.main()
                except SystemExit as failure:
                    code = failure.code
                summary = reports.call_args.args[0] if reports.called else None
                manifests = list(build.glob('ux-artifacts/*/fixture-manifest.json'))
                return {'code': code, 'summary': summary, 'calls': calls,
                        'base_calls': base_factory.call_count,
                        'base_options': base_factory.call_args.kwargs if base_factory.called else None,
                        'extra_calls': extra_factory.call_count,
                        'manifest': json.loads(manifests[0].read_text()) if manifests else None,
                        'stderr': stderr.getvalue()}

    def test_solo_official_uses_only_32_rgb_files_and_reuses_them_for_three_runs(self):
        for quality in (False, True):
            with self.subTest(quality=quality):
                outcome = self.run_cli([palette.OFFICIAL_CASE], quality=quality, runs=3)
                self.assertEqual(outcome['code'], 0)
                self.assertEqual((outcome['base_calls'], outcome['extra_calls']), (0, 1))
                self.assertEqual(len(outcome['calls']), 3)
                self.assertEqual(len({str(folder) for _, folder in outcome['calls']}), 1)
                self.assertTrue(all(folder.parts[-2:] == ('case-fixtures', palette.OFFICIAL_CASE)
                                    for _, folder in outcome['calls']))
                summary = outcome['summary']
                self.assertEqual(summary['inventory_domain'], 'specification')
                self.assertEqual(summary['release_gate'], 'NOT_QUALIFIED')
                profile = summary['fixture_profile']
                self.assertEqual((profile['public_photo_count'], profile['extra_public_photo_count'],
                                  profile['original_checksum_count']), (0, 32, 32))
                self.assertEqual(len(profile['case_profiles']), 1)
                self.assertFalse(profile['large_24mp_generated'])
                self.assertEqual(profile, outcome['manifest']['fixture_profile'])
                self.assertEqual(len(outcome['manifest']['original_sha256']), 32)

    def test_mixed_cases_keep_original_base_counts_and_distinct_routes(self):
        for identifier, count in [('UX-RACE-005', 1), ('UX-SEL-002', 10), ('UX-IMP-009', 20)]:
            with self.subTest(identifier=identifier):
                outcome = self.run_cli([palette.OFFICIAL_CASE, identifier])
                self.assertEqual(outcome['code'], 0)
                self.assertEqual((outcome['base_calls'], outcome['extra_calls']), (1, 1))
                self.assertEqual(outcome['base_options'], {'count': count, 'include_large': False})
                folders = dict(outcome['calls'])
                self.assertEqual(folders[identifier].name, 'public-fixtures-quality')
                self.assertEqual(folders[palette.OFFICIAL_CASE].name, palette.OFFICIAL_CASE)
                self.assertFalse(folders[palette.OFFICIAL_CASE].is_relative_to(folders[identifier]))
                profile = outcome['summary']['fixture_profile']
                self.assertEqual((profile['public_photo_count'], profile['base_original_checksum_count'],
                                  profile['extra_public_photo_count'], profile['original_checksum_count']),
                                 (count, count + 2, 32, count + 34))
                self.assertEqual(len(outcome['manifest']['original_sha256']), count + 34)

    def test_ineligible_or_other_selection_never_creates_extra_profile(self):
        options_list = ({'native': False}, {'override_row': {'scope': '0.6'}},
                        {'extra_flags': ('--suite', 'blocking')},
                        {'extra_flags': ('--require-build-provenance',), 'provenance': 'FAIL'})
        for options in [dict(options, quality=quality) for options in options_list
                        for quality in (False, True)] + [{'denied': True}]:
            with self.subTest(options=options):
                outcome = self.run_cli([palette.OFFICIAL_CASE], **options)
                self.assertNotEqual(outcome['code'], 0)
                self.assertEqual((outcome['base_calls'], outcome['extra_calls']), (0, 0))
                self.assertEqual(outcome['calls'], [])
                if outcome['summary'] is not None:
                    self.assertEqual(outcome['summary']['release_gate'], 'NOT_QUALIFIED')
                    self.assertEqual(outcome['summary']['execution_status'], 'INCOMPLETE')
                    self.assertFalse(any(row['status'] == 'PASS' for row in outcome['summary']['results']))
        outcome = self.run_cli(['UX-SEL-002'])
        self.assertEqual((outcome['code'], outcome['base_calls'], outcome['extra_calls']), (0, 1, 0))

    def test_duplicate_case_selection_has_one_profile_and_union_catches_mutation(self):
        outcome = self.run_cli([palette.OFFICIAL_CASE, palette.OFFICIAL_CASE])
        self.assertEqual((outcome['code'], outcome['extra_calls'], len(outcome['calls'])), (0, 1, 1))
        for mutation in ('bytes', 'remove', 'symlink'):
            with self.subTest(mutation=mutation):
                outcome = self.run_cli([palette.OFFICIAL_CASE], mutate=mutation)
                self.assertEqual(outcome['code'], 1)
                self.assertEqual(outcome['summary']['original_hashes'], 'FAIL')
                self.assertEqual(outcome['summary']['results'][0]['original_integrity'], 'FAIL')
                self.assertEqual(len(outcome['summary']['results'][0]['mutated_originals']), 1)

    def test_integrity_only_case_counts_all_32_originals_once_per_repeat(self):
        outcome = self.run_cli([palette.OFFICIAL_CASE, 'UX-UND-018'], runs=3)
        self.assertEqual(outcome['code'], 0)
        self.assertEqual((outcome['base_calls'], outcome['extra_calls']), (0, 1))
        integrity = [row for row in outcome['summary']['results'] if row['id'] == 'UX-UND-018']
        self.assertEqual(len(integrity), 3)
        self.assertTrue(all(row['original_count'] == 32 and row['status'] == 'PASS' for row in integrity))

    def test_official_policy_and_ci_are_not_supplemental_aliases(self):
        row = next(row for row in runner.INVENTORY if row['id'] == palette.OFFICIAL_CASE)
        self.assertEqual((row['scope'], row['mode'], row['severity']), ('CURRENT', 'REGRESSION', 'P1'))
        self.assertIn(row['id'], runner.NATIVE)
        self.assertIsNone(policy.quality_skip_reason(row))
        for filename in ('macos.yml', 'release.yml'):
            workflow = (runner.ROOT / '.github/workflows' / filename).read_text()
            self.assertIn('run_ux_suite.py --build build --quality-only --case UX-LIB-004 --runs 3', workflow)
            self.assertNotIn('python tests/ux/run_thumbnail_response_regression.py', workflow)
            self.assertIn('build/ux-artifacts/20*/fixture-manifest.json', workflow)
            self.assertNotIn('build/ux-artifacts/20*/case-fixtures/', workflow)


if __name__ == '__main__':
    unittest.main()
