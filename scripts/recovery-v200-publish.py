#!/usr/bin/env python3
"""Publish only a completed, fully gated v2.0.0 recovery build; never replace a release."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import urllib.request
import urllib.parse
import zipfile

REPOSITORY = 'yakut-sekonts/Apertone'
SOURCE = '5b0915a0a6a2d7b565c0ec7cad7fe023619121a5'
TAG_OBJECT = '7fff973d060355090eeb86a00893c96a8ce9869e'
OVERLAY = '572a91b4ef35e9c170567303cc8983025df78554'
OVERLAY_BLOB = 'cc344928f1f00d9386da8a9e867fed3042f162d3'
ASSETS = {'Apertone-2.0.0-macos-arm64.zip', 'Apertone-2.0.0-macos-arm64.dmg',
          'BUILD_INFO.json', 'SHA256SUMS', 'appcast.xml'}


def require(value, message):
    if not value:
        raise ValueError(message)


class NoCredentialRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        redirected = super().redirect_request(req, fp, code, msg, headers, newurl)
        if redirected and urllib.parse.urlsplit(req.full_url).netloc != urllib.parse.urlsplit(newurl).netloc:
            redirected.remove_header('Authorization')
            redirected.remove_header('authorization')
        return redirected


def request(endpoint, *, method='GET', data=None, headers=None, upload=False):
    token = os.environ['GH_TOKEN']
    opener = urllib.request.build_opener(NoCredentialRedirect())
    base = 'https://uploads.github.com/' if upload else 'https://api.github.com/'
    request_headers = {'Authorization': 'Bearer ' + token, 'Accept': 'application/vnd.github+json',
                       'X-GitHub-Api-Version': '2022-11-28', 'User-Agent': 'apertone-v200-recovery'}
    request_headers.update(headers or {})
    return opener.open(urllib.request.Request(base + endpoint, data=data, method=method,
                                              headers=request_headers), timeout=60)


def api(endpoint):
    with request(endpoint) as response:
        return json.load(response)


def artifact(name, artifacts, destination):
    matches = [a for a in artifacts if a['name'] == name and not a['expired']]
    require(len(matches) == 1, f'{name}: artifact missing, expired or ambiguous')
    item = matches[0]
    expected = item.get('digest', '')
    require(re.fullmatch(r'sha256:[0-9a-f]{64}', expected) is not None, 'artifact digest missing')
    archive = destination / (name + '.zip')
    checksum = hashlib.sha256()
    with request(f"repos/{REPOSITORY}/actions/artifacts/{item['id']}/zip") as source, archive.open('wb') as out:
        while block := source.read(1024 * 1024):
            checksum.update(block)
            out.write(block)
    require('sha256:' + checksum.hexdigest() == expected, 'artifact archive digest mismatch')
    with zipfile.ZipFile(archive) as zipped:
        names = zipped.namelist()
        require(len(names) == len(set(names)) and all('/' not in n and '\\' not in n and n not in ('.', '..') for n in names),
                'artifact contains unsafe paths or duplicates')
        require(set(names) == ({'recovery-provenance.json'} if name == 'v200-recovery-provenance' else ASSETS),
                'unexpected artifact content')
        target = destination / name
        target.mkdir()
        for entry in zipped.infolist():
            require(not entry.is_dir() and (entry.external_attr >> 16) & 0o170000 in (0, 0o100000),
                    'artifact includes non-regular entry')
            with zipped.open(entry) as source, (target / entry.filename).open('xb') as out:
                while block := source.read(1024 * 1024):
                    out.write(block)
    return target


def verify_run(run, jobs, evidence, run_id, workflow_sha):
    require(run.get('id') == int(run_id) and run.get('event') == 'workflow_dispatch'
            and run.get('head_branch') == 'main' and run.get('head_sha') == workflow_sha
            and run.get('path') == '.github/workflows/release-recovery-200.yml'
            and run.get('status') == 'completed' and run.get('conclusion') == 'success',
            'recovery workflow did not finish successfully on this reviewed main commit')
    candidates = [job for job in jobs if job.get('name') == 'build-package']
    require(len(candidates) == 1 and candidates[0].get('status') == 'completed'
            and candidates[0].get('conclusion') == 'success', 'recovery build did not succeed')
    require(evidence.get('schema') == 1 and evidence.get('tag') == 'v2.0.0'
            and evidence.get('tag_object') == TAG_OBJECT and evidence.get('product_commit') == SOURCE
            and evidence.get('overlay_commit') == OVERLAY and evidence.get('overlay_blob') == OVERLAY_BLOB
            and evidence.get('overlay_path') == 'tests/ux/TransformExtraScenarios.cpp'
            and evidence.get('workflow_sha') == workflow_sha and evidence.get('run_id') == run_id
            and evidence.get('run_attempt') == str(run.get('run_attempt'))
            and evidence.get('workflow_event') == 'workflow_dispatch'
            and evidence.get('workflow_ref') == f'{REPOSITORY}/.github/workflows/release-recovery-200.yml@refs/heads/main',
            'recovery provenance identity mismatch')
    ux = evidence.get('ux_build_provenance', {})
    require(ux.get('source_commit') == SOURCE and ux.get('source_tree_sha256') == evidence.get('overlay_source_tree_sha256')
            and ux.get('test_executable_sha256') and evidence.get('restored_source_tree_sha256')
            and evidence.get('shipping_binary_sha256') and evidence.get('shipping_build_files')
            and evidence.get('overlay_diff_sha256'),
            'recovery provenance lacks test and shipping attestations')


def no_existing_release():
    page = 1
    while True:
        releases = api(f'repos/{REPOSITORY}/releases?per_page=100&page={page}')
        require(isinstance(releases, list), 'cannot inspect existing releases')
        require(not any(item.get('tag_name') == 'v2.0.0' for item in releases),
                'release already exists, including drafts; refusing any mutation')
        if len(releases) < 100:
            break
        page += 1


def verify_remote_tag():
    ref = api(f'repos/{REPOSITORY}/git/ref/tags/v2.0.0')['object']
    require(ref['type'] == 'tag' and ref['sha'] == TAG_OBJECT, 'remote tag changed')


def json_write(endpoint, payload, method):
    raw = json.dumps(payload).encode('utf-8')
    with request(endpoint, method=method, data=raw, headers={'Content-Type': 'application/json'}) as response:
        return json.load(response)


def release_by_id(release_id):
    release = api(f'repos/{REPOSITORY}/releases/{release_id}')
    require(type(release.get('id')) is int and release['id'] == release_id
            and release.get('tag_name') == 'v2.0.0' and release.get('draft') is True,
            'own draft was deleted or changed; refusing to mutate another release')
    return release


def upload_by_id(release_id, path):
    release_by_id(release_id)
    query = urllib.parse.urlencode({'name': path.name})
    endpoint = f'repos/{REPOSITORY}/releases/{release_id}/assets?{query}'
    with path.open('rb') as stream:
        with request(endpoint, method='POST', data=iter(lambda: stream.read(1024 * 1024), b''), upload=True,
                     headers={'Content-Type': 'application/octet-stream',
                              'Content-Length': str(path.stat().st_size)}) as response:
            return json.load(response)


def publish_by_id(paths, checksums):
    # The API can create a missing tag, so preflight checks the exact annotated
    # object immediately before creating the draft. Protect the tag server-side
    # as well: GitHub offers no atomic "create only if tag object matches" API.
    verify_remote_tag()
    no_existing_release()
    notes = Path('docs/releases/v2.0.0.md').read_text()
    verify_remote_tag()
    draft = json_write(f'repos/{REPOSITORY}/releases',
                       {'tag_name': 'v2.0.0', 'target_commitish': SOURCE, 'name': 'Apertone 2.0.0',
                        'body': notes, 'draft': True, 'prerelease': False}, 'POST')
    release_id = draft.get('id')
    require(type(release_id) is int and release_id > 0 and draft.get('tag_name') == 'v2.0.0'
            and draft.get('draft') is True, 'GitHub did not create the expected draft')
    for path in paths:
        asset = upload_by_id(release_id, path)
        require(asset.get('name') == path.name and asset.get('state') == 'uploaded'
                and asset.get('size') == path.stat().st_size
                and asset.get('digest') == 'sha256:' + checksums[path.name],
                'uploaded asset differs from validated candidate')
    draft = release_by_id(release_id)
    assets = draft.get('assets', [])
    require(len(assets) == len(checksums) and {a.get('name') for a in assets} == set(checksums),
            'draft asset set changed')
    for asset in assets:
        require(asset.get('state') == 'uploaded' and asset.get('digest') == 'sha256:' + checksums[asset['name']],
                'draft asset digest changed')
    verify_remote_tag()
    release_by_id(release_id)
    result = json_write(f'repos/{REPOSITORY}/releases/{release_id}',
                        {'draft': False, 'make_latest': 'true'}, 'PATCH')
    require(result.get('id') == release_id and result.get('tag_name') == 'v2.0.0'
            and result.get('draft') is False, 'unexpected result from release publication')
    print(result['html_url'])


def main():
    require(os.environ['GITHUB_REPOSITORY'] == REPOSITORY and os.environ['GITHUB_REF'] == 'refs/heads/main',
            'publication must run from the repository main branch')
    run_id = os.environ['RECOVERY_RUN_ID']
    require(re.fullmatch(r'[1-9][0-9]*', run_id) is not None, 'invalid recovery run id')
    sha = os.environ['GITHUB_SHA']
    require(subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip() == SOURCE,
            'publisher checkout must be original tag commit')
    verify_remote_tag()
    run = api(f'repos/{REPOSITORY}/actions/runs/{run_id}')
    jobs = api(f'repos/{REPOSITORY}/actions/runs/{run_id}/attempts/{run.get("run_attempt")}/jobs?per_page=100')['jobs']
    artifacts = api(f'repos/{REPOSITORY}/actions/runs/{run_id}/artifacts?per_page=100')['artifacts']
    require(all(a.get('workflow_run', {}).get('head_sha') == sha for a in artifacts
                if a['name'] in ('v200-recovery-provenance', 'macos-release')), 'artifact workflow source mismatch')
    with tempfile.TemporaryDirectory(prefix='apertone-recovery-publish-') as folder:
        temp = Path(folder)
        evdir = artifact('v200-recovery-provenance', artifacts, temp)
        assetsdir = artifact('macos-release', artifacts, temp)
        evidence = json.loads((evdir / 'recovery-provenance.json').read_text())
        verify_run(run, jobs, evidence, run_id, sha)
        hashes = {}
        for path in assetsdir.iterdir():
            with path.open('rb') as stream:
                hashes[path.name] = hashlib.file_digest(stream, 'sha256').hexdigest()
        require(hashes == evidence.get('asset_sha256'), 'assets differ from sealed recovery evidence')
        no_existing_release()
        release = Path('dist/release')
        require(not release.exists(), 'refusing to replace local release assets')
        release.parent.mkdir(parents=True, exist_ok=True)
        assetsdir.rename(release)
        (release / 'RECOVERY_PROVENANCE.json').write_bytes((evdir / 'recovery-provenance.json').read_bytes())
        original = Path('scripts/publish-release.py').read_bytes()
        require(hashlib.sha256(original).hexdigest() == '3399e83030966769b557c61ce4eafc917a474f8fab8a00342c29c1c1af4e2eb5',
                'original publisher differs from audited tag version')
        text = original.decode()
        marker = 'existing = gh("release", "view", tag, "--json", "isDraft", check=False)'
        require(text.count(marker) == 1, 'publisher validation anchor mismatch')
        # Run the original tag's complete local archive/model/feed/checksum validation,
        # stopping before its tag-addressed GitHub mutation logic.
        validation = text.split(marker, 1)[0]
        sys.argv = ['scripts/publish-release.py', '--tag', 'v2.0.0', '--source-commit', SOURCE]
        sys.path.insert(0, str(Path('scripts').resolve()))
        namespace = {'__name__': '__main__', '__file__': 'scripts/publish-release.py'}
        exec(compile(validation, 'scripts/publish-release.py', 'exec'), namespace)
        paths = namespace['paths']
        checksums = namespace['checksums']
        provenance = release / 'RECOVERY_PROVENANCE.json'
        paths.append(provenance)
        checksums[provenance.name] = hashlib.sha256(provenance.read_bytes()).hexdigest()
        publish_by_id(paths, checksums)


if __name__ == '__main__':
    main()
