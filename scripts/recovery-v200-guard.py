#!/usr/bin/env python3
"""Bind a v2.0.0 recovery build to its immutable tag and one UX-only overlay."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

TAG = 'v2.0.0'
TAG_OBJECT = '7fff973d060355090eeb86a00893c96a8ce9869e'
SOURCE = '5b0915a0a6a2d7b565c0ec7cad7fe023619121a5'
OVERLAY_COMMIT = '572a91b4ef35e9c170567303cc8983025df78554'
OVERLAY_BLOB = 'cc344928f1f00d9386da8a9e867fed3042f162d3'
OVERLAY_PATH = 'tests/ux/TransformExtraScenarios.cpp'


def git(*arguments, binary=False):
    result = subprocess.check_output(['git', *arguments])
    return result if binary else result.decode().strip()


def require(condition, description):
    if not condition:
        raise ValueError(description)


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def tracked_changes():
    output = subprocess.check_output(['git', 'status', '--porcelain=v1', '--untracked-files=all'])
    return set(output.decode().splitlines())


def verify_identity():
    require(git('rev-parse', 'HEAD') == SOURCE, 'HEAD must be tag commit')
    require(git('cat-file', '-t', f'refs/tags/{TAG}') == 'tag', 'tag must remain annotated')
    require(git('rev-parse', f'refs/tags/{TAG}') == TAG_OBJECT, 'tag object changed')
    require(git('rev-parse', f'refs/tags/{TAG}^{{commit}}') == SOURCE, 'tag commit changed')
    require(git('rev-parse', f'{OVERLAY_COMMIT}^') == SOURCE, 'unexpected overlay parent')
    require(git('diff', '--name-only', SOURCE, OVERLAY_COMMIT).splitlines() == [OVERLAY_PATH],
            'overlay changes more than the approved test file')
    require(git('rev-parse', f'{OVERLAY_COMMIT}:{OVERLAY_PATH}') == OVERLAY_BLOB, 'overlay blob changed')
    subprocess.run(['git', 'merge-base', '--is-ancestor', SOURCE, 'origin/main'], check=True)


def verify_overlay():
    verify_identity()
    require(tracked_changes() == {f' M {OVERLAY_PATH}'}, 'unexpected working tree changes')
    require(git('hash-object', OVERLAY_PATH) == OVERLAY_BLOB, 'overlay content changed')


def source_fingerprint():
    names = subprocess.check_output(['git', 'ls-files', '-co', '--exclude-standard', '-z']).split(b'\0')
    result = hashlib.sha256()
    for name in sorted(set(names) - {b''}):
        result.update(name + b'\0')
        path = Path(os.fsdecode(name))
        result.update(hashlib.sha256(path.read_bytes()).digest() if path.is_file() else b'DELETED')
    return result.hexdigest()


def shipping_hashes(build):
    app = build / 'Apertone.app'
    require(app.is_dir() and not app.is_symlink(), 'shipping bundle missing')
    result = {}
    for directory, dirs, files in os.walk(app, followlinks=False):
        for name in sorted(dirs + files):
            path = Path(directory) / name
            rel = path.relative_to(build).as_posix()
            if path.is_symlink():
                result[rel] = {'link': os.readlink(path)}
            elif path.is_file():
                result[rel] = {'sha256': digest(path), 'mode': path.stat().st_mode & 0o777}
            else:
                require(path.is_dir(), 'unexpected special file in shipping bundle')
    raw = build / 'libluma_raw.dylib'
    require(raw.is_file() and not raw.is_symlink(), 'shipping LibRaw library missing')
    result[raw.name] = {'sha256': digest(raw), 'mode': raw.stat().st_mode & 0o777}
    return result


def prepare(evidence):
    verify_identity()
    require(not tracked_changes(), 'starting tree must be clean')
    original = git('rev-parse', f'{SOURCE}:{OVERLAY_PATH}')
    Path(OVERLAY_PATH).write_bytes(git('show', f'{OVERLAY_COMMIT}:{OVERLAY_PATH}', binary=True))
    verify_overlay()
    data = {'schema': 1, 'tag': TAG, 'tag_object': TAG_OBJECT, 'product_commit': SOURCE,
            'overlay_commit': OVERLAY_COMMIT, 'overlay_path': OVERLAY_PATH,
            'overlay_blob': OVERLAY_BLOB, 'original_blob': original,
            'overlay_source_tree_sha256': source_fingerprint(),
            'workflow_sha': os.environ['GITHUB_SHA'], 'run_id': os.environ['GITHUB_RUN_ID'],
            'run_attempt': os.environ['GITHUB_RUN_ATTEMPT'],
            'workflow_ref': os.environ['GITHUB_WORKFLOW_REF'],
            'workflow_event': os.environ['GITHUB_EVENT_NAME']}
    require(data['workflow_event'] == 'workflow_dispatch', 'expected workflow_dispatch')
    evidence.write_text(json.dumps(data, indent=2) + '\n')


def record_build(evidence, build):
    data = json.loads(evidence.read_text())
    verify_overlay()
    require(data.get('overlay_source_tree_sha256') == source_fingerprint(), 'source changed during product build')
    data['shipping_build_files'] = shipping_hashes(build)
    evidence.write_text(json.dumps(data, indent=2) + '\n')


def restore(evidence, build):
    data = json.loads(evidence.read_text())
    verify_overlay()
    require(data['product_commit'] == SOURCE and data['overlay_blob'] == OVERLAY_BLOB,
            'recovery evidence identity changed')
    provenance = json.loads((build / 'ux-build-provenance.json').read_text())
    require(provenance.get('schema') == 1 and provenance.get('source_commit') == SOURCE,
            'UX build commit does not match original tag')
    require(provenance.get('source_tree_sha256') == source_fingerprint() == data['overlay_source_tree_sha256'],
            'UX build source fingerprint differs from overlay')
    require(provenance.get('test_executable_sha256') == digest(build / 'luma_ux_tests'),
            'UX executable differs from attested binary')
    require(data.get('shipping_build_files') == shipping_hashes(build), 'shipping build changed during tests')
    binary = build / 'Apertone.app/Contents/MacOS/Apertone'
    data['ux_build_provenance'] = provenance
    data['shipping_binary_sha256'] = digest(binary)
    data['overlay_diff_sha256'] = hashlib.sha256(git('diff', '--binary', '--', OVERLAY_PATH, binary=True)).hexdigest()
    subprocess.run(['git', 'restore', '--worktree', '--', OVERLAY_PATH], check=True)
    require(not tracked_changes(), 'tree not clean after restoration')
    require(git('hash-object', OVERLAY_PATH) == data['original_blob'], 'original test source not restored')
    data['restored_source_tree_sha256'] = source_fingerprint()
    require(digest(binary) == data['shipping_binary_sha256'] and data['shipping_build_files'] == shipping_hashes(build),
            'shipping build changed on restore')
    evidence.write_text(json.dumps(data, indent=2) + '\n')


def seal(evidence, release, build):
    verify_identity()
    require(not tracked_changes(), 'source changed before upload')
    data = json.loads(evidence.read_text())
    require(data.get('restored_source_tree_sha256') == source_fingerprint(), 'restored source tree changed')
    require(data.get('shipping_binary_sha256') and data.get('shipping_build_files') == shipping_hashes(build),
            'shipping build changed before upload')
    expected = {'Apertone-2.0.0-macos-arm64.zip', 'Apertone-2.0.0-macos-arm64.dmg',
                'BUILD_INFO.json', 'SHA256SUMS', 'appcast.xml'}
    require({path.name for path in release.iterdir()} == expected, 'unexpected asset set')
    info = json.loads((release / 'BUILD_INFO.json').read_text())
    require(info.get('source_commit') == SOURCE and info.get('version') == '2.0.0', 'asset source mismatch')
    data['asset_sha256'] = {name: digest(release / name) for name in sorted(expected)}
    evidence.write_text(json.dumps(data, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stage', choices=('prepare', 'record-build', 'restore', 'seal'))
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--build', type=Path, default=Path('build'))
    parser.add_argument('--release', type=Path, default=Path('dist/release'))
    args = parser.parse_args()
    {'prepare': lambda: prepare(args.evidence), 'record-build': lambda: record_build(args.evidence, args.build),
     'restore': lambda: restore(args.evidence, args.build),
     'seal': lambda: seal(args.evidence, args.release, args.build)}[args.stage]()


if __name__ == '__main__':
    main()
