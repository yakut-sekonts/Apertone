"""Reviewed execution policy; specification mode/severity remain unchanged."""
import json
from pathlib import Path


POLICY = json.loads(Path(__file__).with_suffix('.json').read_text())
if POLICY.get('schema') != 1:
    raise ValueError('Unsupported UX quality-policy schema')
QUALITY_ALLOWLIST = frozenset(POLICY['quality_allowlist'])
DEFERRED_LOAD = POLICY['deferred_load']
DEFERRED_MODES = frozenset(('STRESS', 'SOAK', 'PERF_LOCAL', 'CHAOS'))


def quality_skip_reason(row):
    """Return a skip classification/reason; unknown adapters fail closed."""
    if row['mode'] in DEFERRED_MODES:
        return 'deferred_load', 'Specification mode ' + row['mode']
    if row['id'] in DEFERRED_LOAD:
        evidence = DEFERRED_LOAD[row['id']]
        return 'deferred_load', evidence['reason'] + ' [' + evidence['source'] + ']'
    if row['id'] not in QUALITY_ALLOWLIST:
        return 'policy_unreviewed', 'Adapter workload requires source review before quality-only execution'
    return None


def quality_fixture_count(rows):
    """Use a fresh small profile; deferred-only selections generate no fixtures."""
    if not rows:
        return 0
    overrides = POLICY['fixture_photo_overrides']
    return max(overrides.get(row['id'], POLICY['default_quality_photo_count']) for row in rows)
