#!/bin/bash -e

python3 fenton_plots.py
python3 wave_sim_plots.py
python3 wave_theories_plots.py --formats png svg pgf --output-dir . --basename wave_sim-theories
python3 wave_spectrum_plots.py
