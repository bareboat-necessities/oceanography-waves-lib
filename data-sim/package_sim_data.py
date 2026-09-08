#!/usr/bin/env python3
"""Build interchangeable surface and vessel archives with identical flat members."""
import argparse
import zipfile
from pathlib import Path


def package(root):
    root = Path(root)
    vessel = root / "vessel-rao-28ft"
    original = {p.name: p for p in root.glob("*.csv")}
    response = {p.name: p for p in vessel.glob("*.csv")}
    wave_names = {n for n in original if n.startswith(("wave_data_", "wave_spectrum_"))}
    if not wave_names or set(response) != wave_names:
        raise ValueError(f"Vessel CSV members differ: missing={sorted(wave_names-set(response))}, "
                         f"extra={sorted(set(response)-wave_names)}")
    for name, path in response.items():
        with original[name].open() as a, path.open() as b:
            if a.readline() != b.readline():
                raise ValueError(f"CSV schema differs: {name}")
        if name.startswith("wave_spectrum_") and original[name].read_bytes() != path.read_bytes():
            raise ValueError(f"Incident spectrum differs: {name}")
    outputs = []
    for filename, overrides in [("sim-data-files.zip", {}),
                                ("sim-data-files-vessel-rao-28ft.zip", response)]:
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
