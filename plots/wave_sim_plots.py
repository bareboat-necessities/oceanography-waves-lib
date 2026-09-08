#!/usr/bin/env python3
"""Plot either surface-particle or vessel-RAO CSVs, with explicit dataset labels."""
import argparse
from pathlib import Path
import matplotlib as mpl
mpl.use('Agg')
import matplotlib.pyplot as plt
import pandas as pd
from plot_sampling import get_decimation_step

WAVE_TYPES = ('gerstner', 'jonswap', 'fenton', 'pmstokes', 'cnoidal')
HEIGHTS = (0.27, 1.5, 4.0, 8.5)
COLORS = ('#167a3e', '#2365b0', '#8055ad', '#bd2929')
CHARTS = {
    'worldframe': [('Displacement [m]', ['disp_x', 'disp_y', 'disp_z']),
                   ('Velocity [m/s]', ['vel_x', 'vel_y', 'vel_z']),
                   ('Acceleration [m/s²]', ['acc_x', 'acc_y', 'acc_z'])],
    'imu_acc': [(f'Specific force {c} [m/s²]', [f'acc_b{c}']) for c in 'xyz'],
    'imu_gyro': [(f'Body rate {c} [rad/s]', [f'gyro_{c}']) for c in 'xyz'],
    'euler': [(f'{c.capitalize()} [degrees]', [f'{c}_deg']) for c in ('roll', 'pitch', 'yaw')],
}


def load_cases(directory, wave_type, seconds):
    cases = []
    for height in HEIGHTS:
        paths = sorted(directory.glob(f'wave_data_{wave_type}_H{height:.3f}_*.csv'))
        if len(paths) != 1:
            raise ValueError(f'Expected one {wave_type} H={height} record in {directory}, found {len(paths)}')
        # Read only the plotted interval, not the entire 20-minute record four times.
        frame = pd.read_csv(paths[0], nrows=int(seconds * 200) + 1)
        required = {'time', *(c for groups in CHARTS.values() for _, cols in groups for c in cols)}
        if not required.issubset(frame.columns) or frame.empty:
            raise ValueError(f'Missing motion/IMU data: {paths[0]}')
        frame = frame[frame.time <= seconds].iloc[::get_decimation_step()]
        cases.append((height, frame))
    return cases


def plot_wave_type(wave_type, args):
    cases = load_cases(args.input_dir, wave_type, args.seconds)
    regular = wave_type in ('gerstner', 'fenton', 'cnoidal')
    dataset = f'{args.vessel_length_ft} ft sailboat RAO' if args.vessel_rao else 'Surface / particle model'
    for name, groups in CHARTS.items():
        if regular and not args.vessel_rao and name != 'worldframe':
            continue
        fig, axes = plt.subplots(3, 1, figsize=(8, 9), sharex=True)
        fig.suptitle(f'{wave_type.capitalize()} — {dataset}', fontsize=15)
        for (height, frame), color in zip(cases, COLORS):
            for ax, (label, columns) in zip(axes, groups):
                cols = [columns[-1]] if regular and not args.vessel_rao and name == 'worldframe' else columns
                for j, col in enumerate(cols):
                    component = col.rsplit('_', 1)[-1] if len(cols) > 1 else ''
                    ax.plot(frame.time, frame[col], color=color, linestyle=('-', '--', ':')[j],
                            linewidth=1.0, label=f'H={height:g} m {component}'.strip())
                ax.set_ylabel(label)
                ax.grid(alpha=0.3)
        axes[0].legend(fontsize=9, ncol=4)
        axes[-1].set_xlabel('Time [s]')
        fig.tight_layout(rect=(0, 0, 1, 0.96))
        for extension in args.formats:
            path = args.output_dir / f'{args.prefix}{wave_type}_{name}.{extension}'
            fig.savefig(path, bbox_inches='tight')
            print(path)
        plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input-dir', type=Path, default=Path('.'))
    parser.add_argument('--output-dir', type=Path, default=Path('.'))
    parser.add_argument('--vessel-rao', action='store_true')
    parser.add_argument('--vessel-length-ft', type=int, choices=(28,34,42,50), default=28)
    parser.add_argument('--prefix', default='')
    parser.add_argument('--seconds', type=float, default=60)
    parser.add_argument('--formats', nargs='+', choices=('pgf', 'svg', 'png', 'pdf'), default=['pgf', 'svg'])
    args = parser.parse_args()
    if not 0 < args.seconds <= 1200:
        parser.error('--seconds must be in (0, 1200]')
    args.output_dir.mkdir(parents=True, exist_ok=True)
    mpl.rcParams.update({'font.family': 'DejaVu Serif', 'font.size': 10,
                         'pgf.texsystem': 'xelatex', 'pgf.rcfonts': False,
                         'pgf.preamble': r'\usepackage{unicode-math}\setmainfont{DejaVu Serif}\setmathfont{Latin Modern Math}',
                         'svg.fonttype': 'none'})
    for wave_type in WAVE_TYPES:
        plot_wave_type(wave_type, args)


if __name__ == '__main__':
    main()
