"""Owned tiny files and mocked commands only; never build or launch the application."""
import argparse
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import plistlib
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import native_bundle_provenance as bundle
import run_ux_suite as runner
import build_ux as builder


def make_bundle(build):
    directory = build / 'luma_ux_tests.app'
    executable = directory / 'Contents/MacOS/luma_ux_tests'
    executable.parent.mkdir(parents=True)
    executable.write_bytes(b'owned fake executable, never executed\n')
    executable.chmod(0o755)
    values = {'CFBundleIdentifier': 'app.luma.ux-tests', 'CFBundleExecutable': 'luma_ux_tests',
              'CFBundleName': 'LumaUXTests', 'CFBundleDisplayName': 'LumaUXTests',
              'CFBundlePackageType': 'APPL', 'CFBundleVersion': '0.5.1'}
    plist = directory / 'Contents/Info.plist'
    plist.write_bytes(plistlib.dumps(values))
    (build / 'luma_ux_tests').symlink_to(executable)
    return executable, plist, values


def snapshot(build):
    cli = build / 'luma_ux_tests'
    return {'source_tree_sha256': 'unchanged-source',
            'test_executable_sha256': hashlib.sha256(cli.read_bytes()).hexdigest() if cli.is_file() else None,
            'native_bundle_info': bundle.native_bundle_info(build), 'commit': 'fixture'}


def proof(build, state):
    (build / 'ux-build-provenance.json').write_text(json.dumps({'schema': 1, **state}))


class BundleEvidenceTests(unittest.TestCase):
    def test_flat_legacy_absent_and_none_are_compatible(self):
        with tempfile.TemporaryDirectory() as temporary:
            build = Path(temporary).resolve()
            self.assertIsNone(bundle.native_bundle_info(build))
            (build / 'luma_ux_tests').write_bytes(b'old flat bytes, not executed')
            self.assertIsNone(bundle.native_bundle_info(build))
            self.assertEqual(bundle.launch_executable(build), build / 'luma_ux_tests')
            state = snapshot(build)
            for record_has_field in (False, True):
                for snapshot_has_field in (False, True):
                    with self.subTest(record=record_has_field, snapshot=snapshot_has_field):
                        recorded, current = dict(state), dict(state)
                        if not record_has_field:
                            recorded.pop('native_bundle_info')
                        if not snapshot_has_field:
                            current.pop('native_bundle_info')
                        proof(build, recorded)
                        self.assertEqual(runner.build_provenance(build, current)['status'], 'PASS')

    def test_exact_generated_identity_canonical_cli_and_digests(self):
        with tempfile.TemporaryDirectory() as temporary:
            build = Path(temporary).resolve()
            executable, plist, _ = make_bundle(build)
            value = bundle.native_bundle_info(build)
            self.assertEqual(value['status'], 'PASS')
            self.assertEqual(value['bundle_identifier'], 'app.luma.ux-tests')
            self.assertEqual(value['canonical_executable'], str(executable))
            self.assertEqual(value['cli_path'], str(build / 'luma_ux_tests'))
            self.assertEqual(value['info_plist_sha256'], hashlib.sha256(plist.read_bytes()).hexdigest())
            self.assertEqual(value['executable_sha256'], hashlib.sha256(executable.read_bytes()).hexdigest())
            self.assertEqual(bundle.launch_executable(build), executable)
            state = snapshot(build)
            proof(build, state)
            self.assertEqual(runner.build_provenance(build, state)['status'], 'PASS')

    def test_old_flat_proof_cannot_attest_new_bundle(self):
        with tempfile.TemporaryDirectory() as temporary:
            build = Path(temporary).resolve()
            make_bundle(build)
            state = snapshot(build)
            for metadata in ('absent', None):
                with self.subTest(metadata=metadata):
                    old = dict(state)
                    if metadata == 'absent':
                        old.pop('native_bundle_info')
                    else:
                        old['native_bundle_info'] = metadata
                    proof(build, old)
                    self.assertEqual(runner.build_provenance(build, state)['status'], 'UNVERIFIED')

    def test_snapshot_binary_digest_must_agree_with_actual_bundle_executable(self):
        with tempfile.TemporaryDirectory() as temporary:
            build = Path(temporary).resolve()
            make_bundle(build)
            state = {**snapshot(build), 'test_executable_sha256': 'invented binary digest'}
            proof(build, state)
            self.assertEqual(runner.build_provenance(build, state)['status'], 'UNVERIFIED')

    def test_bundle_present_with_missing_info_or_executable_remains_invalid(self):
        for missing in ('plist', 'executable'):
            with self.subTest(missing=missing), tempfile.TemporaryDirectory() as temporary:
                build = Path(temporary).resolve()
                executable, plist, _ = make_bundle(build)
                (plist if missing == 'plist' else executable).unlink()
                self.assertEqual(bundle.native_bundle_info(build)['status'], 'INVALID')

    def test_plist_bytes_drift_invalidates_even_stale_snapshot_and_same_binary(self):
        with tempfile.TemporaryDirectory() as temporary:
            build = Path(temporary).resolve()
            _, plist, values = make_bundle(build)
            before = snapshot(build)
            proof(build, before)
            values['CFBundleVersion'] = '0.5.2'
            plist.write_bytes(plistlib.dumps(values, fmt=plistlib.FMT_BINARY))
            after = snapshot(build)
            self.assertEqual(after['test_executable_sha256'], before['test_executable_sha256'])
            self.assertNotEqual(after['native_bundle_info']['info_plist_sha256'], before['native_bundle_info']['info_plist_sha256'])
            self.assertEqual(after['native_bundle_info']['status'], 'PASS')
            for supplied in (before, after):
                self.assertEqual(runner.build_provenance(build, supplied)['status'], 'UNVERIFIED')

    def test_actual_executable_bytes_drift_and_changed_proof_path_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            build = Path(temporary).resolve()
            executable, _, _ = make_bundle(build)
            before = snapshot(build)
            proof(build, before)
            executable.write_bytes(b'changed fake binary, never executed')
            self.assertEqual(runner.build_provenance(build, snapshot(build))['status'], 'UNVERIFIED')
            fresh = snapshot(build)
            altered = {**fresh, 'native_bundle_info': {**fresh['native_bundle_info'], 'canonical_executable': '/wrong/executable'}}
            proof(build, altered)
            self.assertEqual(runner.build_provenance(build, fresh)['status'], 'UNVERIFIED')

    def test_bad_plist_identity_and_corruption_stay_invalid(self):
        corruptions = [b'garbage', b'<?xml version="1.0"?><plist><dict>', plistlib.dumps([])]
        for payload in corruptions:
            with self.subTest(payload=payload), tempfile.TemporaryDirectory() as temporary:
                build = Path(temporary).resolve()
                _, plist, _ = make_bundle(build)
                plist.write_bytes(payload)
                self.assertEqual(bundle.native_bundle_info(build)['status'], 'INVALID')
        for key in ('CFBundleIdentifier', 'CFBundleExecutable', 'CFBundleName', 'CFBundleDisplayName', 'CFBundlePackageType'):
            with self.subTest(key=key), tempfile.TemporaryDirectory() as temporary:
                build = Path(temporary).resolve()
                _, plist, values = make_bundle(build)
                values[key] = 'wrong'
                plist.write_bytes(plistlib.dumps(values))
                state = snapshot(build)
                self.assertEqual(state['native_bundle_info']['status'], 'INVALID')
                proof(build, state)  # Matching invalid bytes still must not qualify.
                self.assertEqual(runner.build_provenance(build, state)['status'], 'UNVERIFIED')

    def test_wrong_missing_flat_or_alias_cli_and_aliased_bundle_files_rejected(self):
        for mode in ('wrong', 'missing', 'flat', 'relative', 'alias', 'plist-symlink', 'exe-symlink', 'bundle-symlink'):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as temporary:
                build = Path(temporary).resolve()
                executable, plist, _ = make_bundle(build)
                cli = build / 'luma_ux_tests'
                if mode in ('wrong', 'missing', 'flat', 'relative', 'alias'):
                    cli.unlink()
                    if mode == 'wrong':
                        other = build / 'other'
                        other.write_bytes(executable.read_bytes())
                        cli.symlink_to(other)
                    elif mode == 'flat':
                        cli.write_bytes(executable.read_bytes())
                    elif mode == 'relative':
                        cli.symlink_to(executable.relative_to(build))
                    elif mode == 'alias':
                        cli.symlink_to(executable.parent / '..' / 'MacOS' / executable.name)
                elif mode in ('plist-symlink', 'exe-symlink'):
                    path = plist if mode == 'plist-symlink' else executable
                    other = path.with_name(path.name + '.actual')
                    path.rename(other)
                    path.symlink_to(other)
                else:
                    directory = build / 'luma_ux_tests.app'
                    other = build / 'other.app'
                    directory.rename(other)
                    directory.symlink_to(other, target_is_directory=True)
                self.assertEqual(bundle.native_bundle_info(build)['status'], 'INVALID')
                with self.assertRaises(ValueError):
                    bundle.launch_executable(build)

    def test_dangling_cli_without_bundle_is_invalid_not_flat_legacy(self):
        with tempfile.TemporaryDirectory() as temporary:
            build = Path(temporary).resolve()
            (build / 'luma_ux_tests').symlink_to(build / 'missing.app/Contents/MacOS/luma_ux_tests')
            self.assertEqual(bundle.native_bundle_info(build)['status'], 'INVALID')

    def test_fingerprint_reports_bundle_plist_changes_independent_of_source_binary(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            build = root / 'build'
            build.mkdir()
            _, plist, values = make_bundle(build)
            (root / 'fixture.cpp').write_bytes(b'unchanged tracked source')
            def command(arguments, **kwargs):
                return SimpleNamespace(returncode=0, stdout=b'fixture.cpp\0' if '-z' in arguments else 'mock command')
            with patch.object(runner, 'ROOT', root), patch.object(runner.subprocess, 'run', side_effect=command):
                before = runner.fingerprint(build)
                values['CFBundleVersion'] = '0.5.2'
                plist.write_bytes(plistlib.dumps(values))
                after = runner.fingerprint(build)
            self.assertEqual(before['source_tree_sha256'], after['source_tree_sha256'])
            self.assertEqual(before['test_executable_sha256'], after['test_executable_sha256'])
            self.assertNotEqual(before['native_bundle_info'], after['native_bundle_info'])


class BuilderAndLaunchTests(unittest.TestCase):
    def test_builder_records_after_generated_metadata_allows_first_migration(self):
        with tempfile.TemporaryDirectory() as temporary:
            build = Path(temporary).resolve()
            make_bundle(build)
            after = snapshot(build)
            before = {**after, 'native_bundle_info': None, 'test_executable_sha256': None}
            with (patch.object(builder, 'fingerprint', side_effect=[before, after]),
                  patch.object(builder.subprocess, 'run', return_value=SimpleNamespace(returncode=0)) as cmake,
                  patch.object(builder.fcntl, 'flock'),
                  patch.object(builder.tempfile, 'gettempdir', return_value=str(build)),
                  patch('sys.argv', ['build_ux.py', '--build', str(build)]), patch('builtins.print')):
                self.assertEqual(builder.main(), 0)
            recorded = json.loads((build / 'ux-build-provenance.json').read_text())
            self.assertEqual(recorded['native_bundle_info'], after['native_bundle_info'])
            self.assertEqual(recorded['test_executable_sha256'], after['test_executable_sha256'])
            cmake.assert_called_once()

    def test_builder_rejects_invalid_after_metadata_or_source_drift(self):
        for invalid in ('metadata', 'sources'):
            with self.subTest(invalid=invalid), tempfile.TemporaryDirectory() as temporary:
                build = Path(temporary).resolve()
                (build / 'luma_ux_tests').write_bytes(b'fake flat executable')
                after = snapshot(build)
                before = dict(after)
                if invalid == 'metadata':
                    after['native_bundle_info'] = {'status': 'INVALID', 'reason': 'fixture invalid'}
                else:
                    before['source_tree_sha256'] = 'old source'
                proof(build, before)
                with (patch.object(builder, 'fingerprint', side_effect=[before, after]),
                      patch.object(builder.subprocess, 'run', return_value=SimpleNamespace(returncode=0)),
                      patch.object(builder.fcntl, 'flock'),
                      patch.object(builder.tempfile, 'gettempdir', return_value=str(build)),
                      patch('sys.argv', ['build_ux.py', '--build', str(build)]), patch('sys.stderr', io.StringIO())):
                    with self.assertRaises(SystemExit):
                        builder.main()
                self.assertFalse((build / 'ux-build-provenance.json').exists())

    def test_execute_uses_actual_bundle_path_and_preserves_flat_path_without_launch(self):
        for bundled in (False, True):
            with self.subTest(bundled=bundled), tempfile.TemporaryDirectory() as temporary:
                build = Path(temporary).resolve()
                if bundled:
                    expected, _, _ = make_bundle(build)
                else:
                    expected = build / 'luma_ux_tests'
                    expected.write_bytes(b'never executed flat fixture')
                process = SimpleNamespace(pid=456, returncode=0, poll=lambda: 0)
                arguments = argparse.Namespace(build=build, timeout=30)
                row = {'id': 'UX-NAV-001', 'severity': 'P1', 'scope': 'CURRENT', 'mode': 'BLOCKING', 'scenario': 'Mocked command routing only'}
                destination = build / 'case'
                with patch.object(runner.subprocess, 'Popen', return_value=process) as popen:
                    runner.execute(row, destination, arguments, build / 'fixtures')
                self.assertEqual(Path(popen.call_args.args[0][0]), expected)
                self.assertEqual(json.loads((destination / 'command.json').read_text())[0], str(expected))

    def test_invalid_bundle_never_reaches_popen(self):
        with tempfile.TemporaryDirectory() as temporary:
            build = Path(temporary).resolve()
            _, plist, _ = make_bundle(build)
            plist.write_bytes(b'corrupt')
            arguments = argparse.Namespace(build=build, timeout=30)
            with patch.object(runner.subprocess, 'Popen') as popen:
                with self.assertRaises(ValueError):
                    runner.execute({'id': 'UX-NAV-001'}, build / 'case', arguments, build / 'fixtures')
            popen.assert_not_called()


if __name__ == '__main__':
    unittest.main()
