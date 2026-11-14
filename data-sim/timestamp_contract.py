#!/usr/bin/env python3
"""Check the exact 200 Hz sample clock in generated CSVs or packaged ZIPs."""
import argparse
import io
import math
import zipfile
from pathlib import Path

SAMPLE_RATE = 200


def validate_timestamps(lines, name, duration=None):
    if next(lines, '').split(',')[0] != 'time':
        raise ValueError(f'{name}: missing time column')
    first = 2 if Path(name).name.startswith('wave_data_fenton_') else 0
    count = 0
    for count, line in enumerate(lines, 1):
        actual = float(line.partition(',')[0])
        expected = (first + count - 1) / SAMPLE_RATE
        if actual != expected:
            raise ValueError(f'{name}: row {count+1}: time {actual!r}, expected {expected!r}')
    if not count:
        raise ValueError(f'{name}: empty time series')
    if duration is not None:
        steps = math.floor(duration * SAMPLE_RATE) if first else round(duration * SAMPLE_RATE)
        if first:
            # Compare in the same double domain as the generator's endpoint.
            if (steps + 1) / SAMPLE_RATE <= duration:
                steps += 1
            if steps / SAMPLE_RATE > duration:
                steps -= 1
        expected_count = steps - 1 if first else steps
        if count != expected_count:
            raise ValueError(f'{name}: {count} samples, expected {expected_count}')
    return count


def audit(path, duration):
    if path.suffix == '.zip':
        with zipfile.ZipFile(path) as archive:
            names = [n for n in archive.namelist() if n.startswith('wave_data_') and n.endswith('.csv')]
            if len(names) != 20:
                raise ValueError(f'{path}: expected 20 wave records, found {len(names)}')
            for name in names:
                with archive.open(name) as raw, io.TextIOWrapper(raw) as source:
                    validate_timestamps(source, name, duration)
    else:
        with path.open() as source:
            validate_timestamps(source, path.name, duration)
    print(f'PASS: exact timestamps in {path}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('paths', type=Path, nargs='+')
    parser.add_argument('--duration', type=float, default=1200)
    args = parser.parse_args()
    for path in args.paths:
        audit(path, args.duration)
