#!/usr/bin/env python3
"""Package charts, preserving PNG sidecars referenced by PGF files."""
import argparse
from pathlib import Path
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('directory', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
files = sorted(p for p in args.directory.iterdir() if p.suffix in ('.pgf', '.svg', '.png'))
if not files or not any(p.suffix == '.svg' for p in files):
    raise SystemExit('No SVG charts to package')
with zipfile.ZipFile(args.output, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
    for path in files:
        archive.write(path, path.name)
print(f'Packaged {len(files)} chart files in {args.output}')
