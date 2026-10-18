"""Strict CLI routing/record validation for UX-UND-017 only; no UI or oracle model."""
import hashlib
import json
from pathlib import Path
import re

IDENTIFIER = 'UX-UND-017'
MODEL = 'schema8-timeline-cap200-v1'
GENERATOR = 'splitmix64-block64-tail400-v1'
HEX = re.compile(r'[0-9a-f]{64}')
MAX_REPLAY_BYTES = 128 * 1024 * 1024


def _integer(value, minimum, maximum):
    return type(value) is int and minimum <= value <= maximum


def replay_metadata(path):
    """Validate all record structure/checksums here; native independently regenerates the full plan."""
    path = Path(path).resolve()
    if not path.is_file() or path.stat().st_size > MAX_REPLAY_BYTES:
        raise ValueError('Replay must be a readable file within the 128 MiB evidence bound')
    # Bound the bytes actually read as well as the earlier stat: a file may grow
    # between those operations. Never allocate the whole changed file first.
    with path.open('rb') as stream:
        raw = stream.read(MAX_REPLAY_BYTES + 1)
    if len(raw) > MAX_REPLAY_BYTES:
        raise ValueError('Replay grew beyond the 128 MiB evidence bound during reading')
    lines = raw.splitlines(keepends=True)
    if len(lines) < 3 or any(not line.endswith(b'\n') or line.endswith(b'\r\n') for line in lines):
        raise ValueError('Replay is truncated or lacks canonical LF-terminated records')
    records = []
    for number, line in enumerate(lines, 1):
        value = json.loads(line)
        canonical = json.dumps(value, ensure_ascii=False, sort_keys=True,
                               separators=(',', ':'), allow_nan=False).encode() + b'\n'
        if type(value) is not dict or line != canonical:
            raise ValueError(f'Replay line {number} is not a canonical JSON object')
        records.append(value)
    header, footer = records[0], records[-1]
    expected_header = {'kind', 'schema', 'case', 'model', 'generator', 'seed', 'actions',
                       'document_schema', 'application_version', 'qt_version', 'fixture_sha256',
                       'initial_hash'}
    seed = header.get('seed')
    count = header.get('actions')
    if (set(header) != expected_header or header.get('kind') != 'header'
            or type(header.get('schema')) is not int or header['schema'] != 1
            or header.get('case') != IDENTIFIER or header.get('model') != MODEL
            or header.get('generator') != GENERATOR or header.get('document_schema') != 8
            or type(header.get('document_schema')) is not int
            or not isinstance(seed, str) or not re.fullmatch(r'0|[1-9][0-9]{0,19}', seed)
            or int(seed) > 2**64 - 1 or not _integer(count, 1, 100000)
            or type(header.get('fixture_sha256')) is not list or len(header['fixture_sha256']) != 10
            or any(type(value) is not str or not HEX.fullmatch(value) for value in header['fixture_sha256'])
            or type(header.get('initial_hash')) is not str or not HEX.fullmatch(header['initial_hash'])
            or any(type(header.get(key)) is not str or not header[key]
                   for key in ('application_version', 'qt_version'))):
        raise ValueError('Replay header is unsupported or malformed')
    if len(records) != count + 2:
        raise ValueError('Replay action count does not match its header')
    previous = header['initial_hash']
    action_keys = {'kind', 'index', 'op', 'control', 'delta', 'before', 'after', 'undo', 'redo', 'changed'}
    for index, row in enumerate(records[1:-1], 1):
        if (set(row) != action_keys or row.get('kind') != 'action'
                or not _integer(row.get('index'), index, index)
                or row.get('op') not in ('edit', 'undo', 'redo')
                or type(row.get('delta')) is not int or type(row.get('changed')) is not bool
                or any(type(row.get(key)) is not str or not HEX.fullmatch(row[key])
                       for key in ('before', 'after'))
                or not _integer(row.get('undo'), 0, 200) or not _integer(row.get('redo'), 0, 200)
                or row['undo'] + row['redo'] > 200 or row['before'] != previous
                or row['changed'] != (row['before'] != row['after'])):
            raise ValueError(f'Replay action {index} has an invalid index/state/operand contract')
        if row['op'] == 'edit':
            if row['control'] not in ('contrast', 'shadows', 'vibrance') or row['delta'] not in (-1, 1):
                raise ValueError(f'Replay edit {index} uses an unsupported control/delta')
        elif row['control'] != '' or row['delta'] != 0:
            raise ValueError(f'Replay history action {index} contains edit operands')
        previous = row['after']
    last = records[-2]
    if (set(footer) != {'kind', 'actions', 'prefix_sha256', 'final_hash', 'undo', 'redo'}
            or footer.get('kind') != 'footer' or not _integer(footer.get('actions'), count, count)
            or footer.get('prefix_sha256') != hashlib.sha256(b''.join(lines[:-1])).hexdigest()
            or footer.get('final_hash') != previous or footer.get('undo') != last['undo']
            or footer.get('redo') != last['redo']
            or not _integer(footer.get('undo'), 0, 200) or not _integer(footer.get('redo'), 0, 200)):
        raise ValueError('Replay footer/checksum/final state does not match all action records')
    return {'path': path, 'seed': int(seed), 'actions': count, 'sha256': hashlib.sha256(raw).hexdigest()}


def configure_history_chaos(args, parser):
    """Never silently ignore a replay/seed/count, nor pretend other CHAOS IDs are adapted."""
    requested = args.seed is not None or args.actions is not None
    if args.seed is not None and not _integer(args.seed, 0, 2**64 - 1):
        parser.error('--seed must be an unsigned 64-bit integer')
    if args.actions is not None and not _integer(args.actions, 1, 100000):
        parser.error('--actions must be 1..100000 (bounded evidence/resource contract)')
    if args.replay:
        if args.suite not in (None, 'chaos') or args.case not in ([], [IDENTIFIER]) or args.phase:
            parser.error('--replay supports only UX-UND-017; conflicting suite/case/phase is forbidden')
        if args.release_qualification:
            parser.error('Replay is one scoped case and cannot request full release qualification')
        try:
            metadata = replay_metadata(args.replay)
        except (OSError, ValueError, TypeError, OverflowError, RecursionError) as error:
            parser.error('Replay preflight failed: ' + str(error))
        if args.seed is not None and args.seed != metadata['seed']:
            parser.error('Explicit --seed does not match the immutable replay header')
        if args.actions is not None and args.actions != metadata['actions']:
            parser.error('Explicit --actions does not match the immutable replay header')
        args.seed, args.actions, args.replay = metadata['seed'], metadata['actions'], metadata['path']
        args.suite, args.case = 'chaos', [IDENTIFIER]
    else:
        args.suite = args.suite or 'all'
    if args.suite == 'chaos' and args.case != [IDENTIFIER]:
        parser.error('Only scoped history chaos is executable; use --suite chaos --case UX-UND-017')
    selected = ((not args.case or IDENTIFIER in args.case)
                and args.suite in ('all', 'chaos') and args.phase is None)
    if (requested or args.case == [IDENTIFIER]) and not selected:
        parser.error('--seed/--actions/UX-UND-017 would be ignored by this suite/case/phase selection')
    args.seed = 123456 if args.seed is None else args.seed
    args.actions = 10000 if args.actions is None else args.actions
    if args.release_qualification and selected and args.actions < 10000:
        parser.error('Full release qualification cannot replace the 10000-action baseline with a short smoke run')
