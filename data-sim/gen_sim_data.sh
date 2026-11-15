#!/bin/bash -e

make clean
make -j4 all

# run each wave height parallel
seq 0 3 | xargs -n1 -P4 ./waves_sim

# Same incident cases, additional vessel response in a separate directory.
printf '%s\n' 28 34 42 50 | xargs -n1 -P4 ./waves_sim --vessel-length-ft

# Additional underway cases: 5 kn, northward mean heading, +/-3 deg / 20 s.
printf '%s\n' 28 34 42 50 | xargs -n1 -P4 ./waves_sim --vessel-moving --vessel-length-ft

# Separate head-sea cases: 5 kn south, the same +/-3 deg / 20 s steering.
printf '%s\n' 28 34 42 50 | xargs -n1 -P4 ./waves_sim --vessel-moving-south --vessel-length-ft
