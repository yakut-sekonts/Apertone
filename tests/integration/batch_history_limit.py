"""Exercise the real per-photo history byte limit on a disposable catalog."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'assets'))
from generate_dng import create_dng

with tempfile.TemporaryDirectory(prefix='apertone-history-limit-') as temporary:
    source = Path(temporary) / 'source.DNG'
    create_dng(source)
    qml = Path(__file__).resolve().parents[2] / 'apps/desktop/qml'
    subprocess.run([sys.argv[1], str(source), str(qml), '--history-size-limit'], check=True,
                   env={**os.environ, 'QT_QPA_PLATFORM': 'offscreen',
                        'QT_QUICK_BACKEND': 'software'}, timeout=900)
