# oceanography-waves-lib

A C++ header-first library for simulating and analyzing ocean surface waves across deterministic and spectral models.

The project includes:
- **Wave model implementations** (trochoidal/Gerstner, cnoidal, Fenton, JONSWAP, Pierson–Moskowitz, and helpers).
- **Simulation and plotting utilities** for generating CSV outputs and figures.
- **Test and validation executables** for model behavior and spectrum checks.

## Results

Results and generated PDF documentation from main development branch are located at:

https://github.com/bareboat-necessities/oceanography-waves-lib/releases/tag/vTest

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
- `DirectionalSpread.h`
- `SeaMetrics.h`
- `WavesCategories.h`

These headers expose utilities for:
- Surface elevation and derivatives
- Particle kinematics (velocity/acceleration)
- Spectral wave generation and diagnostics
- Sea state and derived wave metrics

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


