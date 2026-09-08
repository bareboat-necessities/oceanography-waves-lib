#!/usr/bin/env python3
"""Build interchangeable surface and vessel archives with identical flat members."""
import argparse
import zipfile
from pathlib import Path


def package(root):
    root = Path(root)
    original = {p.name: p for p in root.glob("*.csv")}
    wave_names = {n for n in original if n.startswith(("wave_data_", "wave_spectrum_"))}
    variants = [("sim-data-files.zip", {})]
    for feet in (28, 34, 42, 50):
        vessel = root / f"vessel-rao-{feet}ft"
        response = {p.name: p for p in vessel.glob("*.csv")}
        if not wave_names or set(response) != wave_names:
            raise ValueError(f"{feet} ft vessel CSV members differ: missing={sorted(wave_names-set(response))}, "
                             f"extra={sorted(set(response)-wave_names)}")
        for name, path in response.items():
            with original[name].open() as a, path.open() as b:
                if a.readline() != b.readline():
                    raise ValueError(f"{feet} ft CSV schema differs: {name}")
            if name.startswith("wave_spectrum_") and original[name].read_bytes() != path.read_bytes():
                raise ValueError(f"{feet} ft incident spectrum differs: {name}")
        variants.append((f"sim-data-files-vessel-rao-{feet}ft.zip", response))
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
    package(parser.parse_args().directory)
