#pragma once

// Eulerian incident elevation: a cos(k (x cos(beta) + y sin(beta)) - omega t + phase).
// Explicit k also permits harmonics of an existing regular-wave profile.
struct WaveHarmonic {
    double amplitude;
    double omega;
    double wavenumber;
    double direction;
    double phase;
};
