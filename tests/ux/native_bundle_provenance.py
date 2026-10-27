"""Narrow identity evidence for the generated macOS UX test bundle, not code signing."""
import hashlib
from pathlib import Path
import plistlib
from xml.parsers.expat import ExpatError

EXECUTABLE_NAME = 'luma_ux_tests'
BUNDLE_IDENTIFIER = 'app.luma.ux-tests'
BUNDLE_NAME = 'ApertoneUXTests'


def native_bundle_info(build):
    """Return None for a legacy flat build; malformed bundle layouts stay explicit."""
    build = Path(build).resolve()
    cli = build / EXECUTABLE_NAME
    bundle = build / (EXECUTABLE_NAME + '.app')
    if not bundle.exists() and not bundle.is_symlink() and not cli.is_symlink():
        return None
    try:
        def regular(path):
            if not path.is_file() or path.is_symlink() or path.resolve() != path:
                raise ValueError('Generated bundle file is missing or aliased: ' + str(path))
            return path.read_bytes()

        if not bundle.is_dir() or bundle.is_symlink() or bundle.resolve() != bundle:
            raise ValueError('Generated UX bundle directory is missing or aliased')
        executable = bundle / 'Contents' / 'MacOS' / EXECUTABLE_NAME
        plist = bundle / 'Contents' / 'Info.plist'
        plist_bytes = regular(plist)
        values = plistlib.loads(plist_bytes)
        expected = {'CFBundleIdentifier': BUNDLE_IDENTIFIER,
                    'CFBundleExecutable': EXECUTABLE_NAME, 'CFBundlePackageType': 'APPL',
                    'CFBundleName': BUNDLE_NAME, 'CFBundleDisplayName': BUNDLE_NAME}
        if not isinstance(values, dict) or any(values.get(key) != value for key, value in expected.items()):
            raise ValueError('Generated UX Info.plist identity does not match its declared bundle contract')
        executable_bytes = regular(executable)
        # CMake generates this absolute link. A flat file beside a bundle, a dangling
        # link, another target or aliases within its target are not that public interface.
        if (not cli.is_symlink() or cli.readlink() != executable
                or cli.resolve(strict=True) != executable):
            raise ValueError('Generated UX CLI symlink does not target the canonical bundle executable')
        return {'status': 'PASS', 'bundle_path': str(bundle), 'info_plist_path': str(plist),
                'info_plist_sha256': hashlib.sha256(plist_bytes).hexdigest(),
                'bundle_identifier': BUNDLE_IDENTIFIER, 'bundle_executable': EXECUTABLE_NAME,
                'bundle_name': BUNDLE_NAME, 'bundle_display_name': BUNDLE_NAME,
                'package_type': 'APPL', 'cli_path': str(cli),
                'canonical_executable': str(executable),
                'executable_sha256': hashlib.sha256(executable_bytes).hexdigest()}
    except (OSError, ValueError, TypeError, plistlib.InvalidFileException, ExpatError) as error:
        return {'status': 'INVALID', 'expected_bundle_path': str(bundle), 'reason': str(error)}


def launch_executable(build):
    """Launch the actual bundle path so CoreFoundation need not infer it from a symlink."""
    build = Path(build).resolve()
    bundle = native_bundle_info(build)
    if bundle is not None:
        if bundle.get('status') != 'PASS':
            raise ValueError('Refusing invalid generated UX bundle: ' + bundle.get('reason', 'unknown'))
        return Path(bundle['canonical_executable'])
    return (build / EXECUTABLE_NAME).resolve()
