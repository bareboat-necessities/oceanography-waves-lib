# oceanography-waves-lib

A C++ header-first library for simulating and analyzing ocean surface waves across deterministic and spectral models.

The project includes:
- **Wave model implementations** (trochoidal/Gerstner, cnoidal, Fenton, JONSWAP, Pierson–Moskowitz, and helpers).
- **Simulation and plotting utilities** for generating CSV outputs and figures.
- **Test and validation executables** for model behavior and spectrum checks.

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

### 1) Run test executable

```bash
cd tests
make
./tests
```

Test entry points are configured in `tests/tests.cpp` by preprocessor switches (`FENTON_TEST`, `JONSWAP_TEST`, `PM_STOKES_TEST`).

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
