#!/usr/bin/env python3
"""Exercise all 20 scenarios and the actual archive path without a long simulation."""
import csv
import importlib.util
import math
import subprocess
import tempfile
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("packaging", REPO / "data-sim/package_sim_data.py")
packaging = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packaging)


def check():
    with tempfile.TemporaryDirectory(prefix="vessel-archive-") as temp:
        root = Path(temp)
        response = root / "vessel-rao-28ft"
        executable = REPO / "data-sim/waves_sim"
        for args in [[], ["--vessel-rao"]]:
            target = response if args else root
            subprocess.run([str(executable), *args, "--duration", "0.05", "--output-dir", str(target)],
                           check=True, stdout=subprocess.DEVNULL)
        original = sorted(root.glob("wave_data_*.csv"))
        assert len(original) == 20
        assert len(list(response.glob("*.csv"))) == 28
        for path in original:
            with path.open() as a, (response / path.name).open() as b:
                source, vessel = list(csv.DictReader(a)), list(csv.DictReader(b))
            assert source and len(source) == len(vessel), path.name
            assert [s["time"] for s in source] == [s["time"] for s in vessel], path.name
            assert any(a != b for a, b in zip(source, vessel)), path.name
            for row in vessel:
                assert len(row) == 23 and all(math.isfinite(float(v)) for v in row.values()), path.name
                w, x, y, z = [float(row[f"q_wb_zu_{c}"]) for c in "wxyz"]
                assert abs(w*w+x*x+y*y+z*z-1) < 3e-6, path.name
                # Independently rotate specific force back into world axes.
                rotation = [[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
                            [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
                            [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]]
                body = [float(row[f"acc_b{c}"]) for c in "xyz"]
                recovered = [sum(rotation[j][i]*body[j] for j in range(3)) for i in range(3)]
                recovered[2] -= 9.80665
                assert max(abs(recovered[i]-float(row[f"acc_{c}"]))
                           for i, c in enumerate("xyz")) < 2e-4, path.name
        # Auxiliary plotting tables must also survive a drop-in replacement.
        (root / "fenton_aux.csv").write_text("x,eta\n0,1\n")
        paths = packaging.package(root)
        with zipfile.ZipFile(paths[0]) as a, zipfile.ZipFile(paths[1]) as b:
            assert a.namelist() == b.namelist()
            assert all("/" not in name for name in b.namelist())
            for name in b.namelist():
                assert a.read(name).splitlines()[0] == b.read(name).splitlines()[0]
                if not name.startswith("wave_data_"):
                    assert a.read(name) == b.read(name), name
        missing = next(response.glob("wave_data_*.csv"))
        missing.unlink()
        try:
            packaging.package(root)
        except ValueError:
            pass
        else:
            raise AssertionError("Packaging silently accepted a missing vessel record")
    print("PASS: all 20 vessel records, clocks, IMU frames, and replacement archive members")


if __name__ == "__main__":
    check()
