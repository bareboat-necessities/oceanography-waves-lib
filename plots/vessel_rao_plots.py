#!/usr/bin/env python3
"""Plot production displacement RAOs sampled by vessel_rao_response."""
import argparse
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

DOFS = ('Surge', 'Sway', 'Heave', 'Roll', 'Pitch', 'Yaw')
HEADINGS = (0, 45, 90, 135, 180)


def render(table, feet, output, prefix, formats):
    data = table[table.feet == feet].copy()
    if len(data) != 200 * 37 * 6 or data.duplicated(['frequency_hz', 'beta_deg', 'dof']).any():
        raise ValueError(f'Incomplete or duplicate response grid for {feet} ft')
    if not np.isfinite(data.to_numpy()).all():
        raise ValueError('Nonfinite response samples')
    data['gain'] = np.hypot(data.real, data.imag)
    data['phase'] = np.degrees(np.angle(data.real + 1j * data.imag))
    # Phase is undefined for zero response. Suppress numerical sin(pi) residues too.
    data.loc[data.gain < 1e-10, 'phase'] = np.nan
    output.mkdir(parents=True, exist_ok=True)
    for kind in ('gain', 'phase', 'heading'):
        fig, axes = plt.subplots(2, 3, figsize=(12, 7.8), constrained_layout=True)
        fig.suptitle(f'{feet} ft sailboat RAO: {kind}\n'
                     'Estimated zero-speed surrogate; deep-water k = omega squared / g', fontsize=14)
        for j, ax in enumerate(axes.flat):
            subset = data[data.dof == j]
            unit = 'm/m' if j < 3 else 'rad/m'
            ax.set_title(DOFS[j] + (' (fixed heading)' if j == 5 else ''))
            ax.set_xlabel('Wave frequency [Hz]')
            if kind == 'heading':
                grid = subset.pivot(index='beta_deg', columns='frequency_hz', values='gain')
                mesh = ax.pcolormesh(grid.columns, grid.index, grid.values,
                                     shading='auto', cmap='viridis', vmin=0,
                                     vmax=max(float(grid.values.max()), 1e-12) if j != 5 else 1, rasterized=True)
                bar = fig.colorbar(mesh, ax=ax, label=f'Gain [{unit}]')
                if j == 5:
                    bar.set_ticks([0])
                    ax.text(.5, .5, 'Zero yaw response', color='white', ha='center',
                            transform=ax.transAxes)
                ax.set_ylabel('Propagation relative to bow [deg]')
            else:
                for beta in HEADINGS:
                    curve = subset[subset.beta_deg == beta].sort_values('frequency_hz')
                    y = curve[kind].to_numpy().copy()
                    # Leave gaps at wrapped phase jumps rather than drawing false ramps.
                    if kind == 'phase':
                        jumps = np.flatnonzero(np.abs(np.diff(y)) > 180) + 1
                        y[jumps] = np.nan
                    ax.plot(curve.frequency_hz, y, label=f'{beta} deg', linewidth=1.5,
                            linestyle='--' if beta > 90 else '-')
                ax.set_ylabel(f'Gain [{unit}]' if kind == 'gain' else 'Phase [deg]')
                ax.grid(alpha=0.25)
                if kind == 'gain':
                    ax.set_ylim(bottom=0)
                else:
                    ax.set_ylim(-185, 185)
                    ax.set_yticks([-180, -90, 0, 90, 180])
                    if j == 5:
                        ax.text(.5, .5, 'Undefined: zero yaw response', ha='center', transform=ax.transAxes)
            ax.set_xlim(.005, 1)
        if kind != 'heading':
            axes.flat[0].legend(fontsize=8, ncol=2)
        fig.supxlabel('0 deg: following; 90 deg: beam; 180 deg: head waves. '
                      + ('Wrapped phase; positive phase is lag for exp(-i omega t).' if kind == 'phase'
                         else 'Gain is per metre of incident elevation amplitude.'), fontsize=10)
        for fmt in formats:
            fig.savefig(output / f'{prefix}response_{kind}.{fmt}', dpi=160)
        plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('--output-dir', type=Path, default=Path('.'))
    parser.add_argument('--vessel-length-ft', type=int, choices=(28, 34, 42, 50), default=28)
    parser.add_argument('--prefix')
    parser.add_argument('--formats', nargs='+', choices=('png', 'svg', 'pgf'), default=['png', 'svg', 'pgf'])
    args = parser.parse_args()
    plt.rcParams.update({'svg.fonttype': 'none', 'font.family': 'DejaVu Sans',
                         'pgf.texsystem': 'xelatex'})
    prefix = args.prefix if args.prefix is not None else (
        'vessel_rao_' if args.vessel_length_ft == 28 else f'vessel_rao_{args.vessel_length_ft}ft_')
    render(pd.read_csv(args.input), args.vessel_length_ft, args.output_dir, prefix, args.formats)


if __name__ == '__main__':
    main()
