#!/usr/bin/env python3
"""Summarize generated vessel records; emit evidence outside the replacement ZIP."""
import argparse
import csv
import hashlib
import math
import sys
from pathlib import Path


def audit(directory, output):
    files = sorted(Path(directory).glob("wave_data_*.csv"))
    if len(files) != 20:
        raise ValueError(f"Expected 20 full vessel records, found {len(files)}")
    writer = csv.writer(output)
    writer.writerow(["file", "samples", "max_acc_norm_mps2", "above_g_samples",
                     "max_abs_roll_deg", "max_abs_pitch_deg", "sha256"])
    for path in files:
        count = above = 0
        peak = roll = pitch = 0.0
        digest = hashlib.sha256()
        with path.open("rb") as raw:
            for block in iter(lambda: raw.read(1024*1024), b""):
                digest.update(block)
        with path.open() as source:
            for row in csv.DictReader(source):
                if len(row) != 23 or any(v is None or v == "" for v in row.values()):
                    raise ValueError(f"Incomplete sample in {path.name}, row {count+2}")
                values = {k: float(v) for k, v in row.items()}
                if not all(math.isfinite(v) for v in values.values()):
                    raise ValueError(f"Nonfinite sample in {path.name}, row {count+2}")
                norm = math.sqrt(sum(values[f"acc_{c}"]**2 for c in "xyz"))
                peak = max(peak, norm)
                above += norm > 9.80665
                roll = max(roll, abs(values["roll_deg"]))
                pitch = max(pitch, abs(values["pitch_deg"]))
                count += 1
        expected = 239795 if path.name.startswith("wave_data_fenton_") else 240000
        if count != expected:
            raise ValueError(f"Incomplete full record: {path.name}: {count} rows, expected {expected}")
        writer.writerow([path.name, count, f"{peak:.12g}", above,
                         f"{roll:.12g}", f"{pitch:.12g}", digest.hexdigest()])
        output.flush()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", nargs="?", default="vessel-rao-28ft", type=Path)
    audit(parser.parse_args().directory, sys.stdout)
