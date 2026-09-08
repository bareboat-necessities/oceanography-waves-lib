#!/bin/bash -e

make clean
make -j4 all

# run each wave height parallel
seq 0 3 | xargs -n1 -P4 ./waves_sim

# Same incident cases, additional vessel response in a separate directory.
./waves_sim --vessel-rao
