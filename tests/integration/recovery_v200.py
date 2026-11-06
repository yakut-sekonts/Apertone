#!/usr/bin/env python3
"""Contract tests for immutable-source recovery; never access the network."""
import importlib.util
import hashlib
import io
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest
import zipfile
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


guard = module('recovery_guard', ROOT / 'scripts/recovery-v200-guard.py')
publisher = module('recovery_publisher', ROOT / 'scripts/recovery-v200-publish.py')


class Guard(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.old = Path.cwd()
        os.chdir(self.temp.name)
        self.addCleanup(os.chdir, self.old)
        self.git('init', '-q')
        self.git('config', 'user.name', 'test')
        self.git('config', 'user.email', 'test@example.com')
        Path('.gitignore').write_text('build/\ndist/\n')
        Path('tests/ux').mkdir(parents=True)
        Path('tests/ux/test.cpp').write_text('old test\n')
        Path('product.cpp').write_text('product\n')
        Path('scripts').mkdir()
        Path('scripts/verify-bundle.py').write_bytes(guard.OLD_SCHEMA_CHECK + b'\n' + guard.OLD_THUMBNAIL_CHECK + b'\n')
        self.git('add', '.')
        self.git('commit', '-qm', 'product')
        source = self.git('rev-parse', 'HEAD')
        self.git('tag', '-a', 'vtest', '-m', 'tag')
        tag_object = self.git('rev-parse', 'refs/tags/vtest')
        self.git('update-ref', 'refs/remotes/origin/main', source)
        Path('tests/ux/test.cpp').write_text('new test\n')
        self.git('commit', '-qam', 'test only')
        fix = self.git('rev-parse', 'HEAD')
        blob = self.git('rev-parse', 'HEAD:tests/ux/test.cpp')
        self.git('checkout', '-q', '--detach', source)
        for name, value in {'TAG': 'vtest', 'TAG_OBJECT': tag_object, 'SOURCE': source,
                            'OVERLAY_COMMIT': fix, 'OVERLAY_BLOB': blob,
                            'OVERLAY_PATH': 'tests/ux/test.cpp'}.items():
            patch = mock.patch.object(guard, name, value)
            patch.start()
            self.addCleanup(patch.stop)
        patch = mock.patch.dict(os.environ, {'GITHUB_SHA': 'control', 'GITHUB_RUN_ID': '123',
                               'GITHUB_RUN_ATTEMPT': '1', 'GITHUB_WORKFLOW_REF': 'workflow',
                               'GITHUB_EVENT_NAME': 'workflow_dispatch'})
        patch.start()
        self.addCleanup(patch.stop)
        self.evidence = Path(self.temp.name).parent / (Path(self.temp.name).name + '-evidence.json')
        self.addCleanup(self.evidence.unlink, missing_ok=True)

    @staticmethod
    def git(*args):
        return subprocess.check_output(['git', *args], text=True).strip()

    def setup_build(self):
        exe = Path('build/luma_ux_tests')
        exe.parent.mkdir(exist_ok=True)
        exe.write_bytes(b'ux executable')
        shipping = Path('build/Apertone.app/Contents/MacOS/Apertone')
        shipping.parent.mkdir(parents=True)
        shipping.write_bytes(b'shipping')
        Path('build/libluma_raw.dylib').write_bytes(b'libraw')
        guard.record_build(self.evidence, Path('build'))
        attestation = {'schema': 1, 'source_commit': guard.SOURCE,
                       'source_tree_sha256': guard.source_fingerprint(),
                       'test_executable_sha256': guard.digest(exe)}
        Path('build/ux-build-provenance.json').write_text(json.dumps(attestation))
        return shipping

    def test_allowed_overlay_is_restored_after_attested_tests(self):
        guard.prepare(self.evidence)
        self.assertEqual(Path(guard.OVERLAY_PATH).read_text(), 'new test\n')
        self.setup_build()
        guard.restore(self.evidence, Path('build'))
        self.assertEqual(Path(guard.OVERLAY_PATH).read_text(), 'old test\n')
        self.assertEqual(guard.tracked_changes(), set())
        evidence = json.loads(self.evidence.read_text())
        self.assertNotEqual(evidence['overlay_source_tree_sha256'], evidence['restored_source_tree_sha256'])
        self.assertIn('Apertone.app/Contents/MacOS/Apertone', evidence['shipping_build_files'])

    def test_reject_unexpected_product_or_overlay_change(self):
        guard.prepare(self.evidence)
        Path('product.cpp').write_text('tampered\n')
        with self.assertRaisesRegex(ValueError, 'unexpected working tree'):
            guard.verify_overlay()
        self.git('restore', 'product.cpp')
        Path(guard.OVERLAY_PATH).write_text('wrong test\n')
        with self.assertRaisesRegex(ValueError, 'overlay content changed'):
            guard.verify_overlay()

    def test_reject_shipping_modification_during_tests(self):
        guard.prepare(self.evidence)
        shipping = self.setup_build()
        shipping.write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'shipping build changed'):
            guard.restore(self.evidence, Path('build'))

    def test_reject_wrong_attestation(self):
        guard.prepare(self.evidence)
        self.setup_build()
        path = Path('build/ux-build-provenance.json')
        data = json.loads(path.read_text())
        data['source_commit'] = 'different'
        path.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, 'UX build commit'):
            guard.restore(self.evidence, Path('build'))

    def test_seal_rejects_product_change_after_restore(self):
        guard.prepare(self.evidence)
        shipping = self.setup_build()
        guard.restore(self.evidence, Path('build'))
        release = Path('dist/release')
        release.mkdir(parents=True)
        for name in publisher.ASSETS:
            (release / name).write_bytes(b'asset')
        (release / 'BUILD_INFO.json').write_text(json.dumps({'version': '2.0.0', 'source_commit': guard.SOURCE}))
        guard.bundle_patch(self.evidence)
        self.assertIn(guard.NEW_SCHEMA_CHECK, Path(guard.BUNDLE_PATH).read_bytes())
        self.assertIn(guard.NEW_THUMBNAIL_CHECK, Path(guard.BUNDLE_PATH).read_bytes())
        guard.bundle_restore(self.evidence)
        guard.seal(self.evidence, release, Path('build'))
        self.assertEqual(set(json.loads(self.evidence.read_text())['asset_sha256']), publisher.ASSETS)
        shipping.write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'shipping build changed'):
            guard.seal(self.evidence, release, Path('build'))

    def test_bundle_verifier_rejects_tampering(self):
        guard.prepare(self.evidence)
        self.setup_build()
        guard.restore(self.evidence, Path('build'))
        guard.bundle_patch(self.evidence)
        Path(guard.BUNDLE_PATH).write_bytes(b'tampered')
        with self.assertRaisesRegex(ValueError, 'bundle verifier changed'):
            guard.bundle_restore(self.evidence)

    def test_seal_requires_bundle_verification(self):
        guard.prepare(self.evidence)
        self.setup_build()
        guard.restore(self.evidence, Path('build'))
        with self.assertRaisesRegex(ValueError, 'corrected bundle verification missing'):
            guard.seal(self.evidence, Path('dist/release'), Path('build'))

    def test_refuse_changed_tag_and_dirty_initial_tree(self):
        Path('product.cpp').write_text('dirty\n')
        with self.assertRaisesRegex(ValueError, 'starting tree must be clean'):
            guard.prepare(self.evidence)
        self.git('restore', 'product.cpp')
        self.git('tag', '-f', 'vtest', guard.OVERLAY_COMMIT)
        with self.assertRaisesRegex(ValueError, 'tag must remain annotated'):
            guard.prepare(self.evidence)


class Publication(unittest.TestCase):
    def example(self):
        sha, run_id = 'a' * 40, '123'
        run = {'id': 123, 'event': 'workflow_dispatch', 'head_branch': 'main', 'head_sha': sha,
               'path': '.github/workflows/release-recovery-200.yml', 'status': 'completed',
               'conclusion': 'success', 'run_attempt': 2}
        jobs = [{'name': 'build-package', 'status': 'completed', 'conclusion': 'success'}]
        evidence = {'schema': 1, 'tag': 'v2.0.0', 'tag_object': publisher.TAG_OBJECT,
                    'product_commit': publisher.SOURCE, 'overlay_commit': publisher.OVERLAY,
                    'overlay_blob': publisher.OVERLAY_BLOB, 'overlay_path': 'tests/ux/TransformExtraScenarios.cpp',
                    'workflow_sha': sha, 'run_id': run_id, 'run_attempt': '2',
                    'workflow_event': 'workflow_dispatch',
                    'workflow_ref': f'{publisher.REPOSITORY}/.github/workflows/release-recovery-200.yml@refs/heads/main',
                    'ux_build_provenance': {'source_commit': publisher.SOURCE, 'source_tree_sha256': 'tree',
                                            'test_executable_sha256': 'exe'},
                    'overlay_source_tree_sha256': 'tree', 'restored_source_tree_sha256': 'restored',
                    'shipping_binary_sha256': 'binary', 'shipping_build_files': {'file': 'hash'},
                    'overlay_diff_sha256': 'patch', 'bundle_verification_passed': True,
                    'bundle_schema_expected': 23, 'bundle_thumbnail_expected': 'edited-v6', 'bundle_original_sha256': publisher.BUNDLE_ORIGINAL,
                    'bundle_corrected_sha256': publisher.BUNDLE_CORRECTED}
        return run, jobs, evidence, run_id, sha

    def test_requires_successful_correct_run_and_sealed_provenance(self):
        run, jobs, evidence, run_id, sha = self.example()
        publisher.verify_run(run, jobs, evidence, run_id, sha)
        for bad in ('failure', 'cancelled', None):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                publisher.verify_run({**run, 'conclusion': bad}, jobs, evidence, run_id, sha)
        for key, value in [('product_commit', 'wrong'), ('run_attempt', '1'), ('overlay_blob', 'wrong'), ('bundle_corrected_sha256', 'wrong')]:
            with self.subTest(key=key), self.assertRaises(ValueError):
                publisher.verify_run(run, jobs, {**evidence, key: value}, run_id, sha)
        with self.assertRaises(ValueError):
            publisher.verify_run(run, [{'name': 'build-package', 'status': 'completed', 'conclusion': 'failure'}], evidence, run_id, sha)

    def test_any_existing_release_including_draft_forbids_publish(self):
        with mock.patch.object(publisher, 'api', return_value=[{'tag_name': 'v2.0.0', 'draft': True}]):
            with self.assertRaisesRegex(ValueError, 'release already exists'):
                publisher.no_existing_release()
        with mock.patch.object(publisher, 'api', return_value=[]):
            publisher.no_existing_release()

    def test_publish_refuses_draft_replacement_and_writes_by_id(self):
        with tempfile.TemporaryDirectory() as folder:
            asset = Path(folder) / 'asset.zip'
            asset.write_bytes(b'contents')
            checksum = hashlib.sha256(asset.read_bytes()).hexdigest()
            created = {'id': 123, 'tag_name': 'v2.0.0', 'draft': True}
            replacement = {'id': 456, 'tag_name': 'v2.0.0', 'draft': True, 'assets': []}
            uploaded = {'name': asset.name, 'state': 'uploaded', 'size': asset.stat().st_size,
                        'digest': 'sha256:' + checksum}
            with mock.patch.object(publisher, 'verify_remote_tag'), mock.patch.object(publisher, 'no_existing_release'), \
                 mock.patch.object(publisher, 'json_write', return_value=created) as write, \
                 mock.patch.object(publisher, 'upload_by_id', return_value=uploaded) as upload, \
                 mock.patch.object(publisher, 'api', return_value=replacement) as api:
                with self.assertRaisesRegex(ValueError, 'own draft was deleted or changed'):
                    publisher.publish_by_id([asset], {asset.name: checksum})
                self.assertEqual(upload.call_args.args[0], 123)
                self.assertEqual(api.call_args.args[0], f'repos/{publisher.REPOSITORY}/releases/123')
                self.assertEqual(write.call_count, 1, 'must not PATCH replacement')

    def test_publish_finalizes_the_same_draft_id(self):
        with tempfile.TemporaryDirectory() as folder:
            asset = Path(folder) / 'asset.zip'
            asset.write_bytes(b'contents')
            checksum = hashlib.sha256(asset.read_bytes()).hexdigest()
            entry = {'name': asset.name, 'state': 'uploaded', 'size': asset.stat().st_size,
                     'digest': 'sha256:' + checksum}
            draft = {'id': 123, 'tag_name': 'v2.0.0', 'draft': True, 'assets': [entry]}
            published = {'id': 123, 'tag_name': 'v2.0.0', 'draft': False, 'html_url': 'https://example.test'}
            with mock.patch.object(publisher, 'verify_remote_tag'), mock.patch.object(publisher, 'no_existing_release'), \
                 mock.patch.object(publisher, 'json_write', side_effect=[draft, published]) as write, \
                 mock.patch.object(publisher, 'upload_by_id', return_value=entry), \
                 mock.patch.object(publisher, 'api', return_value=draft):
                publisher.publish_by_id([asset], {asset.name: checksum})
                endpoint, body, method = write.call_args.args
                self.assertEqual(endpoint, f'repos/{publisher.REPOSITORY}/releases/123')
                self.assertEqual(method, 'PATCH')
                self.assertFalse(body['draft'])

    def test_upload_path_targets_original_id_only(self):
        with tempfile.TemporaryDirectory() as folder:
            asset = Path(folder) / 'asset.zip'
            asset.write_bytes(b'contents')
            response = io.BytesIO(b'{"name":"asset.zip"}')
            with mock.patch.object(publisher, 'release_by_id') as inspect, \
                 mock.patch.object(publisher, 'request', return_value=response) as send:
                publisher.upload_by_id(123, asset)
                inspect.assert_called_once_with(123)
                self.assertIn('/releases/123/assets?', send.call_args.args[0])
                self.assertEqual(send.call_args.kwargs['method'], 'POST')
                self.assertTrue(send.call_args.kwargs['upload'])

    def test_artifact_rejects_bad_digest_and_paths(self):
        def archive(name, value):
            output = io.BytesIO()
            with zipfile.ZipFile(output, 'w') as zipped:
                zipped.writestr(name, value)
            return output.getvalue()
        for name, valid_digest, accepted in [
                ('recovery-provenance.json', True, True),
                ('recovery-provenance.json', False, False),
                ('../recovery-provenance.json', True, False)]:
            content = archive(name, b'{}')
            digest = hashlib.sha256(content).hexdigest() if valid_digest else '0' * 64
            artifacts = [{'name': 'v200-recovery-provenance', 'expired': False, 'id': 1,
                          'digest': 'sha256:' + digest}]
            with self.subTest(name=name, valid=valid_digest), tempfile.TemporaryDirectory() as folder:
                with mock.patch.object(publisher, 'request', return_value=io.BytesIO(content)):
                    if accepted:
                        path = publisher.artifact('v200-recovery-provenance', artifacts, Path(folder))
                        self.assertEqual((path / name).read_bytes(), b'{}')
                    else:
                        with self.assertRaises(ValueError):
                            publisher.artifact('v200-recovery-provenance', artifacts, Path(folder))


class WorkflowParity(unittest.TestCase):
    def test_all_original_build_steps_preserved(self):
        def steps(text):
            text = text.split('  build-package:\n', 1)[1].split('  publish:\n', 1)[0]
            chunks = re.split(r'(?=^      - (?:name:|uses:))', text, flags=re.M)[1:]
            return {re.search(r'^      - (?:name:|uses:) (.*)$', chunk, re.M).group(1): chunk for chunk in chunks}
        tag_workflow = subprocess.check_output(['git', 'show', 'v2.0.0:.github/workflows/release.yml'], cwd=ROOT, text=True)
        original = steps(tag_workflow)
        candidate = steps((ROOT / '.github/workflows/release-recovery-200.yml').read_text())
        original.pop('Validate release source')
        original['Bounded functional blocking UX checks before packaging'] = original['Bounded functional blocking UX checks before packaging'].replace(
            '--expected-commit "$GITHUB_SHA"', '--expected-commit ' + publisher.SOURCE)
        for name, content in original.items():
            with self.subTest(name=name):
                self.assertIn(name, candidate)
                self.assertEqual(content, candidate[name])


if __name__ == '__main__':
    unittest.main()
