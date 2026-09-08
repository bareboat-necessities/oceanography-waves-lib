# oceanography-waves-lib

A C++ header-first library for simulating and analyzing ocean surface waves across deterministic and spectral models.

The project includes:
- **Wave model implementations** (trochoidal/Gerstner, cnoidal, Fenton, JONSWAP, Pierson–Moskowitz, and helpers).
- **Simulation and plotting utilities** for generating CSV outputs and figures.
- **Test and validation executables** for model behavior and spectrum checks.

## Results

[Release 1.2.3](https://github.com/bareboat-necessities/oceanography-waves-lib/releases/tag/v1.2.3) includes all five simulation datasets, SVG/PGF chart archives, and all PDF documentation.

- [Vessel RAO chart PDF](https://github.com/bareboat-necessities/oceanography-waves-lib/releases/download/v1.2.3/wave_sim_charts_vessel_rao_28ft.pdf)
- [Vessel RAO SVG/PGF charts](https://github.com/bareboat-necessities/oceanography-waves-lib/releases/download/v1.2.3/plot-files-vessel-rao-28ft.zip)
- [Vessel RAO simulation CSVs](https://github.com/bareboat-necessities/oceanography-waves-lib/releases/download/v1.2.3/sim-data-files-vessel-rao-28ft.zip)

- [34 ft RAO simulation CSVs](https://github.com/bareboat-necessities/oceanography-waves-lib/releases/download/v1.2.3/sim-data-files-vessel-rao-34ft.zip)
- [42 ft RAO simulation CSVs](https://github.com/bareboat-necessities/oceanography-waves-lib/releases/download/v1.2.3/sim-data-files-vessel-rao-42ft.zip)
- [50 ft RAO simulation CSVs](https://github.com/bareboat-necessities/oceanography-waves-lib/releases/download/v1.2.3/sim-data-files-vessel-rao-50ft.zip)

The vessel charts cover all five wave families and all four incident heights, including H=4 m. Surface and vessel charts are labeled and packaged separately. A versioned release is published only after tests, full simulation audits, all five plotting jobs and all PDF builds succeed.

<p align="center">
  <img src="./img/samples/spectrum_pmstokes_medium_3d.svg?raw=true" style="max-width: 50%;">
</p>

<p align="center">
  <img src="./img/samples/spectrum_pmstokes_medium_polar.svg?raw=true" style="max-width: 50%;">
</p>

## Repository layout

- `src/` — core wave model headers and shared utilities.
- `tests/` — simple executable-based validation suite.
- `data-sim/` — simulation executable for generating data.
- `plots/` — plotting helpers (C++ and Python scripts).
- `doc/` — LaTeX documentation and model notes.

## Requirements

### C++
- `g++` (tested with modern C++ compilers)
- `Eigen` headers (expected at `/usr/include/eigen3`)

### Optional (for plotting)
- Python 3
- Matplotlib / NumPy (for Python plotting scripts)

## Building and running

### CMake (top-level project)

```bash
cmake -S . -B build -DOWLIB_BUILD_TESTS=ON -DOWLIB_BUILD_DATA_SIM=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Useful CMake options:
- `OWLIB_BUILD_TESTS` (default `ON`) builds `owlib_tests`.
- `OWLIB_BUILD_DATA_SIM` (default `ON`) builds `waves_sim`.
- `OWLIB_BUILD_PLOTS` (default `OFF`) builds `fenton_plots_cpp`.

### 1) Run test executable

```bash
cd tests
make check
```

The test binary now performs assertion-based coverage for directional spreading normalization, Fenton invariants, JONSWAP/PM spectral variance, deterministic seeded states, and invalid-parameter error handling.

For a local CI-equivalent gate, you can also run:

```bash
cd tests
./run_tests.sh
```

### 2) Build simulation binary

```bash
cd data-sim
make
./waves_sim
```

This target is useful for generating wave field/spectrum CSV output for further analysis.

An additional estimated **28-foot fin-keel sailboat RAO** simulation is available:

```bash
./waves_sim --vessel-rao
```

It writes vessel CG motion and consistent IMU/attitude references into
`vessel-rao-28ft/`, using the original CSV filenames and columns. Running
`bash gen_sim_data.sh` generates the surface dataset and all four vessel sizes; `python3 package_sim_data.py`
packages them as `sim-data-files.zip` and **`sim-data-files-vessel-rao-<size>ft.zip`**
with identical member names. CI also includes the existing ancillary plotting
CSVs and uploads the additional ZIP as its own artifact/release asset.
See [vessel RAO parameters, equations and scope](doc/vessel-rao.md). The preset
is an approximate stationary sailboat response, not measured hull data.

### 3) Build plotting helper (C++)

```bash
cd plots
make
./fenton_plots
```

Python plotting utilities are also available in `plots/` (for example `wave_sim_plots.py` and `wave_spectrum_plots.py`).

## Available model headers

Core headers in `src/` include:
- `TrochoidalWave.h`
- `CnoidalWave.h`
- `FentonWaveVectorized.h`
- `Jonswap3dStokesWaves.h`
- `PiersonMoskowitzStokes3D_Waves.h`
- `VesselRao.h`
- `DirectionalSpread.h`
- `SeaMetrics.h`
- `WavesCategories.h`

These headers expose utilities for:
- Surface elevation and derivatives
- Particle kinematics (velocity/acceleration)
- Spectral wave generation and diagnostics
- Sea state and derived wave metrics

## JONSWAP particle model

JONSWAP uses second-order deep-water potential-flow interactions, including
sum and difference frequencies and particle advection. Its position, velocity,
and acceleration describe one consistent particle trajectory. See
[the derivation, physical scope, and migration notes](doc/jonswap-second-order.md).

`getSurfaceSlopes` is Eulerian. Use `getLagrangianSurfaceSlopes` for a
particle-following IMU attitude. Horizontal position now includes Stokes drift;
consumers comparing wave excursions should explicitly account for that drift.
The independent seeded phases and corrected kinematics change generated records.
This is a wave-particle model; vessel response and breaking are outside its scope.

Run the eight 20-minute reference acceleration diagnostics with:

```bash
make -C tests reference_acceleration_audit
tests/reference_acceleration_audit > reference-acceleration-audit.csv
```

The report is a full-record numerical diagnostic, not a universal physical bound.

## Minimal usage example

```cpp
#include <iostream>
#include "TrochoidalWave.h"

int main() {
    TrochoidalWave<double> wave(1.0, 8.0); // amplitude [m], period [s]

    double t = 1.5;
    std::cout << "eta(t) = " << wave.surfaceElevation(t) << "\n";
    std::cout << "u(x0,z0,t) = " << wave.horizontalVelocity(0.0, -2.0, t) << "\n";
}
```

Compile (example):

```bash
g++ -O3 -I./src -I/usr/include/eigen3 your_file.cpp -o your_program
```

## Notes

- Most of the library is header-based, so linking is typically straightforward.
- For reproducible experiments, keep generated CSV and plots versioned separately from source.
- Mathematical derivations and additional background are provided in the `doc/` LaTeX sources.

## License

This project is distributed under the terms of the `LICENSE` file in the repository root.


### Sailboat size variants

The generator also supports **34 ft, 42 ft, and 50 ft** fin-keel RAO presets,
using documented geometric/Froude scaling of the existing 28 ft surrogate.
Run `./waves_sim --vessel-length-ft 34` (or `42`, `50`) from `data-sim`.
The standard generation and packaging workflow produces separate
`sim-data-files-vessel-rao-34ft.zip`, `sim-data-files-vessel-rao-42ft.zip`, and
`sim-data-files-vessel-rao-50ft.zip` with the same CSV filenames and columns as
the 28 ft ZIP. Each size also has a chart ZIP, a PDF, and a full-record audit
in CI artifacts and release 1.2.3. Existing release 1.2.1 is
immutable and contains only the original surface and 28 ft datasets.

See [vessel preset parameters and assumptions](doc/vessel-rao.md#additional-34-ft-42-ft-and-50-ft-presets).
