#!/usr/bin/env python3
"""Check the UX launcher path guard with a symlinked build root."""
from pathlib import Path
import subprocess
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[2] / 'cmake/LinkUxTestExecutable.cmake'
SUFFIX = '/luma_ux_tests.app/Contents/MacOS/luma_ux_tests'


class LauncherPaths(unittest.TestCase):
    def test_symlinked_root_is_canonical_and_reusable(self):
        with tempfile.TemporaryDirectory(prefix='apertone-launcher-') as folder:
            parent = Path(folder)
            real = parent / 'real'
            real.mkdir()
            alias = parent / 'alias'
            alias.symlink_to(real, target_is_directory=True)
            args = ['cmake', f'-DUX_BUILD_ROOT={alias}', f'-DUX_PROJECT_ROOT={real}',
                    f'-DUX_BUNDLE_EXECUTABLE={alias}{SUFFIX}', '-P', str(SCRIPT)]
            subprocess.run(args, check=True, capture_output=True)
            launcher = real / 'luma_ux_tests'
            self.assertTrue(launcher.is_symlink())
            self.assertEqual(launcher.readlink(), Path(str(real.resolve()) + SUFFIX))
            subprocess.run(args, check=True, capture_output=True)

    def test_rejects_executable_outside_build_root(self):
        with tempfile.TemporaryDirectory(prefix='apertone-launcher-') as folder:
            args = ['cmake', f'-DUX_BUILD_ROOT={folder}', f'-DUX_PROJECT_ROOT={folder}',
                    f'-DUX_BUNDLE_EXECUTABLE={folder}/elsewhere', '-P', str(SCRIPT)]
            result = subprocess.run(args, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Unexpected UX bundle executable path', result.stderr)
            self.assertFalse((Path(folder) / 'luma_ux_tests').exists())


if __name__ == '__main__':
    unittest.main()
