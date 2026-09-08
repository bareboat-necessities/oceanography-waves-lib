#!/usr/bin/env python3
"""Require complete, correctly labeled SVG/PGF charts for the selected dataset."""
import sys
from pathlib import Path
import xml.etree.ElementTree as ET


def check(directory, dataset):
    vessel = dataset.startswith('vessel-rao')
    feet = dataset.removeprefix('vessel-rao-').removesuffix('ft') if dataset != 'vessel-rao' else '28'
    prefix = ('vessel_rao_' if feet == '28' else f'vessel_rao_{feet}ft_') if vessel else ''
    expected = []
    for wave in ('gerstner', 'jonswap', 'fenton', 'pmstokes', 'cnoidal'):
        kinds = ('worldframe', 'imu_acc', 'imu_gyro', 'euler') if vessel or wave in ('jonswap', 'pmstokes') else ('worldframe',)
        for kind in kinds:
            base = directory / f'{prefix}{wave}_{kind}'
            for extension in ('.svg', '.pgf'):
                path = base.with_suffix(extension)
                if not path.is_file() or not path.stat().st_size:
                    raise ValueError(f'Missing chart: {path}')
                expected.append(path)
            text = ' '.join(ET.parse(base.with_suffix('.svg')).getroot().itertext())
            label = f'{feet} ft sailboat RAO' if vessel else 'Surface / particle model'
            if label not in text or any(f'H={h} m' not in text for h in ('0.27', '1.5', '4', '8.5')):
                raise ValueError(f'Wrong dataset label or missing height in {base}')
    print(f'PASS: {dataset}: {len(expected)//2} complete charts with all four heights')


if __name__ == '__main__':
    check(Path(sys.argv[1]), sys.argv[2])
