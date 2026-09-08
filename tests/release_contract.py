#!/usr/bin/env python3
import importlib.util
import os
import json
import subprocess
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('release', ROOT / 'ci/release.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)
SHA = 'a'*40
ENV = {'GITHUB_REPOSITORY': 'owner/repo', 'GITHUB_SHA': SHA, 'GITHUB_REF': 'refs/heads/main'}


class ReleaseTests(unittest.TestCase):
    def test_draft_lookup_falls_back_to_authenticated_collection(self):
        draft = {'tag_name': 'v1.2.1', 'draft': True, 'assets': []}
        responses = [
            subprocess.CompletedProcess([], 1, '', 'HTTP 404'),
            subprocess.CompletedProcess([], 0, json.dumps([draft]), ''),
        ]
        with patch.object(release.subprocess, 'run', side_effect=responses) as run:
            self.assertEqual(release.lookup('repos/owner/repo/releases/tags/v1.2.1'), draft)
            self.assertEqual(run.call_args.args[0][-1], 'repos/owner/repo/releases?per_page=100&page=1')

    def test_missing_assets_block_before_publication(self):
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaises(ValueError):
                release.prepare(Path(temp))

    def test_complete_manifest_and_exact_commit_publication(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for name in release.ASSETS:
                (root/name).write_bytes(b'test fixture')
            version, paths = release.prepare(root)
            uploaded = {'assets': [{'name': p.name, 'size': p.stat().st_size, 'state': 'uploaded'} for p in paths]}
            with patch.dict(os.environ, ENV), patch.object(release, 'lookup', side_effect=[None, None, uploaded]), patch.object(release, 'gh') as gh:
                release.publish(version, paths)
                create = gh.call_args_list[0].args
                self.assertIn('--target', create)
                self.assertEqual(create[create.index('--target')+1], SHA)
                self.assertEqual(create[create.index('--title')+1], version)
                self.assertIn('--draft', create)
                self.assertEqual(gh.call_args_list[-1].args[:3], ('release', 'edit', 'v'+version))
            self.assertEqual(len((root/'SHA256SUMS').read_text().splitlines()), len(release.ASSETS))

    def test_wrong_tag_commit_never_moves(self):
        with patch.dict(os.environ, ENV), patch.object(release, 'lookup', side_effect=[None, {'object': {'type':'commit', 'sha':'b'*40}}]), patch.object(release, 'gh') as gh:
            with self.assertRaises(ValueError):
                release.publish('1.2.1', [])
            gh.assert_not_called()

    def test_incomplete_upload_remains_draft(self):
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp)/'asset.zip'
            p.write_bytes(b'data')
            with patch.dict(os.environ, ENV), patch.object(release, 'lookup', side_effect=[None, None, {'assets':[]}]), patch.object(release, 'gh') as gh:
                with self.assertRaises(ValueError):
                    release.publish('1.2.1', [p])
                self.assertFalse(any(call.args[:2] == ('release', 'edit') for call in gh.call_args_list))

    def test_published_version_is_immutable(self):
        with patch.dict(os.environ, ENV), patch.object(release, 'lookup', return_value={'draft':False}), patch.object(release, 'gh') as gh:
            release.publish('1.2.1', [])
            gh.assert_not_called()


if __name__ == '__main__':
    unittest.main()
