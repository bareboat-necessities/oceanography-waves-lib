#!/usr/bin/env python3
"""Build interchangeable surface and vessel archives with identical flat members."""
import argparse
import zipfile
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from timestamp_contract import validate_timestamps


def package(root, duration=1200):
    root = Path(root)
    original = {p.name: p for p in root.glob("*.csv")}
    wave_names = {n for n in original if n.startswith(("wave_data_", "wave_spectrum_"))}
    for name in sorted(wave_names):
        if name.startswith("wave_data_"):
            with original[name].open() as source:
                validate_timestamps(source, name, duration)
    if "wave_tracker_data.csv" in original:
        with original["wave_tracker_data.csv"].open() as source:
            validate_timestamps(source, "wave_tracker_data.csv", 30)
    variants = [("sim-data-files.zip", {})]
    for feet, suffix in ((feet, suffix) for suffix in ("", "-5kn") for feet in (28, 34, 42, 50)):
        vessel = root / f"vessel-rao-{feet}ft{suffix}"
        response = {p.name: p for p in vessel.glob("*.csv")}
        if not wave_names or set(response) != wave_names:
            raise ValueError(f"{feet} ft{suffix} vessel CSV members differ: missing={sorted(wave_names-set(response))}, "
                             f"extra={sorted(set(response)-wave_names)}")
        for name, path in response.items():
            if name.startswith("wave_data_"):
                with path.open() as source:
                    validate_timestamps(source, name, duration)
            with original[name].open() as a, path.open() as b:
                if a.readline() != b.readline():
                    raise ValueError(f"{feet} ft{suffix} CSV schema differs: {name}")
            if name.startswith("wave_spectrum_") and original[name].read_bytes() != path.read_bytes():
                raise ValueError(f"{feet} ft{suffix} incident spectrum differs: {name}")
        variants.append((f"sim-data-files-vessel-rao-{feet}ft{suffix}.zip", response))
    outputs = []
    for filename, overrides in variants:
        output = root / filename
        # Recreate, never append: stale members cannot survive a later run.
        with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for name in sorted(original):
                # Ancillary Fenton plotting tables describe the incident waves.
                archive.write(overrides.get(name, original[name]), arcname=name)
        outputs.append(output)
        print(f"Wrote {output} ({len(original)} identical member names)")
    return outputs


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", nargs="?", default=".", type=Path)
    parser.add_argument("--duration", type=float, default=1200)
    args = parser.parse_args()
    package(args.directory, args.duration)
