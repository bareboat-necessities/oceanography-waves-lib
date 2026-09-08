#!/usr/bin/env python3
"""Validate a complete build's assets, then publish its versioned GitHub release."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
ASSETS = (
    'sim-data-files.zip', 'sim-data-files-vessel-rao-28ft.zip',
    'plot-files.zip', 'plot-files-vessel-rao-28ft.zip',
    'waves.pdf', 'wave_model_fenton.pdf', 'wave_model_gerstner.pdf',
    'wave_model_spectral.pdf', 'wave_sim_charts.pdf', 'wave_sim_charts_vessel_rao_28ft.pdf',
    'reference-acceleration-audit.csv', 'vessel-rao-reference-audit.csv',
)


def prepare(directory):
    version = (ROOT / 'VERSION').read_text().strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise ValueError('VERSION must contain a stable semantic version')
    if f'VERSION {version}' not in (ROOT / 'CMakeLists.txt').read_text():
        raise ValueError('CMake project version and VERSION differ')
    paths = [directory / name for name in ASSETS]
    for path in paths:
        if not path.is_file() or not path.stat().st_size:
            raise ValueError(f'Missing or empty release asset: {path}')
    manifest = directory / 'SHA256SUMS'
    with manifest.open('w') as out:
        for path in paths:
            with path.open('rb') as source:
                digest = hashlib.file_digest(source, 'sha256').hexdigest()
            out.write(f'{digest}  {path.name}\n')
    return version, paths + [manifest]


def gh(*args):
    return subprocess.check_output(['gh', *args], text=True)


def lookup(endpoint):
    result = subprocess.run(['gh', 'api', endpoint], text=True, capture_output=True)
    if result.returncode == 0:
        return json.loads(result.stdout)
    if 'HTTP 404' in result.stderr:
        return None
    raise RuntimeError(result.stderr)


def publish(version, paths):
    repo, sha = os.environ['GITHUB_REPOSITORY'], os.environ['GITHUB_SHA']
    if not re.fullmatch(r'[0-9a-f]{40}', sha):
        raise ValueError('Release requires the exact successfully built commit SHA')
    tag = f'v{version}'
    ref = os.environ['GITHUB_REF']
    if ref != 'refs/heads/main' and ref != f'refs/tags/{tag}':
        raise ValueError('Only main or the matching version tag can publish')
    release = lookup(f'repos/{repo}/releases/tags/{tag}')
    if release and not release['draft']:
        # Future main commits must bump VERSION to publish another release.
        print(f'{tag} is already published; keeping that immutable release unchanged.')
        return
    tagged = lookup(f'repos/{repo}/git/ref/tags/{tag}')
    if tagged:
        obj = tagged['object']
        while obj['type'] == 'tag':
            obj = json.loads(gh('api', f"repos/{repo}/git/tags/{obj['sha']}"))['object']
        if obj['type'] != 'commit' or obj['sha'] != sha:
            raise ValueError(f'{tag} already points to another commit; refusing to move it')
    if release and release['target_commitish'] != sha:
        raise ValueError('Existing draft targets another commit')
    if not release:
        gh('release', 'create', tag, '--repo', repo, '--target', sha, '--title', version,
           '--notes-file', str(ROOT / 'RELEASE_NOTES.md'), '--draft')
    gh('release', 'upload', tag, *map(str, paths), '--repo', repo, '--clobber')
    uploaded = lookup(f'repos/{repo}/releases/tags/{tag}')
    actual = {a['name']: a for a in uploaded['assets']}
    if set(actual) != {p.name for p in paths}:
        raise ValueError('Uploaded release asset names do not match the complete manifest')
    for path in paths:
        asset = actual[path.name]
        if asset['size'] != path.stat().st_size or asset['state'] != 'uploaded':
            raise ValueError(f'Incomplete uploaded asset: {path.name}')
        if asset.get('digest'):
            with path.open('rb') as source:
                digest = 'sha256:' + hashlib.file_digest(source, 'sha256').hexdigest()
            if asset['digest'] != digest:
                raise ValueError(f'Uploaded asset digest mismatch: {path.name}')
    gh('release', 'edit', tag, '--repo', repo, '--draft=false', '--latest')
    print(f'Published {version}: https://github.com/{repo}/releases/tag/{tag}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--publish', action='store_true')
    args = parser.parse_args()
    version, paths = prepare(args.directory)
    print(f'Validated release {version}: {len(paths)} assets')
    if args.publish:
        publish(version, paths)
