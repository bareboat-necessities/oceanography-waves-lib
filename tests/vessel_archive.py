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
        executable = REPO / "data-sim/waves_sim"
        subprocess.run([str(executable), "--duration", "0.05", "--output-dir", str(root)],
                       check=True, stdout=subprocess.DEVNULL)
        original = sorted(root.glob("wave_data_*.csv"))
        assert len(original) == 20
        prior = None
        variants = (("", None, 0), ("-5kn", "--vessel-moving", 0),
                    ("-5kn-south", "--vessel-moving-south", 180))
        for feet, (suffix, option, heading) in ((feet, variant) for variant in variants for feet in (28, 34, 42, 50)):
            response = root / f"vessel-rao-{feet}ft{suffix}"
            subprocess.run([str(executable), "--vessel-length-ft", str(feet), "--duration", "0.05"]
                           + ([option] if option else []), cwd=root, check=True, stdout=subprocess.DEVNULL)
            assert len(list(response.glob("*.csv"))) == 28
            signature = (response / original[0].name).read_bytes()
            assert signature != prior, "Different hulls must produce different motion"
            prior = signature
            for path in original:
                with path.open() as a, (response / path.name).open() as b:
                    source, vessel = list(csv.DictReader(a)), list(csv.DictReader(b))
                assert source and len(source) == len(vessel), path.name
                assert [s["time"] for s in source] == [s["time"] for s in vessel], path.name
                assert any(a != b for a, b in zip(source, vessel)), path.name
                for row in vessel:
                    assert len(row) == 23 and all(math.isfinite(float(v)) for v in row.values()), path.name
                    if option:
                        expected_yaw = heading + 3*math.sin(2*math.pi*float(row["time"])/120)
                        assert abs(float(row["yaw_deg"])-expected_yaw) < (1e-5 if heading else 1e-7)
                        direction = -1 if heading == 180 else 1
                        assert direction*float(row["vel_x"]) > 0, "Moving vessel must advance along selected heading"
                    w, x, y, z = [float(row[f"q_wb_zu_{c}"]) for c in "wxyz"]
                    assert abs(w*w+x*x+y*y+z*z-1) < 3e-6, path.name
                    # Independently rotate specific force back into world axes.
                    rotation = [[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
                                [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
                                [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]]
                    yaw = math.degrees(math.atan2(rotation[0][1], rotation[0][0]))
                    yaw_error = (yaw-float(row["yaw_deg"])+180) % 360-180
                    assert abs(yaw_error) < 2e-5, "Quaternion must agree with yaw across 180 degrees"
                    body = [float(row[f"acc_b{c}"]) for c in "xyz"]
                    recovered = [sum(rotation[j][i]*body[j] for j in range(3)) for i in range(3)]
                    recovered[2] -= 9.80665
                    assert max(abs(recovered[i]-float(row[f"acc_{c}"]))
                               for i, c in enumerate("xyz")) < 2e-4, path.name
        # Auxiliary plotting tables must also survive a drop-in replacement.
        (root / "fenton_aux.csv").write_text("x,eta\n0,1\n")
        with (root / "wave_tracker_data.csv").open("w") as source:
            source.write("Time(s),Displacement(m),Velocity(m/s),Acceleration(m/s²),X_Position(m)\n")
            for step in range(2, 6001):
                source.write(f"{step / 200:.17g},0,0,0,0\n")
        paths = packaging.package(root, duration=0.05)
        assert len(paths) == 13
        assert {p.name for p in paths if "5kn" in p.name} == {
            f"sim-data-files-vessel-rao-{feet}ft-{suffix}.zip"
            for suffix in ("5kn", "5kn-south") for feet in (28, 34, 42, 50)}
        for vessel_zip in paths[1:]:
            with zipfile.ZipFile(paths[0]) as a, zipfile.ZipFile(vessel_zip) as b:
                assert a.namelist() == b.namelist()
                assert all("/" not in name for name in b.namelist())
                for name in b.namelist():
                    if name.startswith("wave_data_"):
                        packaging.validate_timestamps(iter(b.read(name).decode().splitlines()), name, 0.05)
                        assert b.read(name) == (root / vessel_zip.stem.removeprefix("sim-data-files-") / name).read_bytes()
                    if name == "wave_tracker_data.csv":
                        packaging.validate_timestamps(iter(b.read(name).decode().splitlines()), name, 30)
                    assert a.read(name).splitlines()[0] == b.read(name).splitlines()[0]
                    if not name.startswith("wave_data_"):
                        assert a.read(name) == b.read(name), name
        # A valid-looking schema must not hide quantized or drifting clocks.
        corrupt = next(response.glob("wave_data_gerstner_*.csv"))
        saved = corrupt.read_text()
        rows = saved.splitlines(keepends=True)
        rows[2] = "0," + rows[2].partition(",")[2]  # duplicate second sample
        corrupt.write_text("".join(rows))
        try:
            packaging.package(root, duration=0.05)
        except ValueError as error:
            assert "time" in str(error)
        else:
            raise AssertionError("Packaging accepted a duplicate timestamp")
        corrupt.write_text(saved)
        missing = next(response.glob("wave_data_*.csv"))
        missing.unlink()
        try:
            packaging.package(root, duration=0.05)
        except ValueError:
            pass
        else:
            raise AssertionError("Packaging silently accepted a missing vessel record")
        for options in (("--vessel-moving", "--vessel-moving-south"),
                        ("--vessel-moving-south", "--vessel-moving")):
            invalid = subprocess.run([str(executable), *options, "--duration", "0.05"],
                                     cwd=root, capture_output=True, text=True)
            assert invalid.returncode != 0 and "only one" in invalid.stderr
    print("PASS: all 240 vessel records, clocks, IMU frames, heading, and thirteen replacement archives")


if __name__ == "__main__":
    check()
