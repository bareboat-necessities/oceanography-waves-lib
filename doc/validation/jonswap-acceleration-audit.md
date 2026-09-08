# Reference acceleration audit

Each run covers 240,000 samples per record (20 minutes at 200 Hz), with gravity excluded. Frequencies, amplitudes, direction distribution and wave parameters are unchanged. The same-phases replay isolates the physical correction from the independent-phase initialization change.

| Model | Hs (m) | Baseline maximum (m/s²) | Corrected, original phases | Corrected, independent phases | Corrected above-g samples |
|---|---:|---:|---:|---:|---:|
| jonswap | 0.27 | 2.832029 | 1.972806 | 1.997859 | 0 |
| pmstokes | 0.27 | 2.184966 | 2.184966 | 2.184966 | 0 |
| jonswap | 1.50 | 6.723224 | 3.918356 | 4.288856 | 0 |
| pmstokes | 1.50 | 5.190412 | 5.190412 | 5.190412 | 0 |
| jonswap | 4.00 | 9.227155 | 5.265208 | 5.724134 | 0 |
| pmstokes | 4.00 | 6.633228 | 6.633228 | 6.633228 | 0 |
| jonswap | 8.50 | 16.216235 | 7.346311 | 7.058101 | 0 |
| pmstokes | 8.50 | 8.568213 | 8.568213 | 8.568213 | 0 |

The original JONSWAP Hs=8.5 m record reproduces the reported **16.2162348913 m/s²** peak at **270.579993952 s**, and **845** above-g samples. The physical correction with those same phases gives **7.3463107329 m/s²**, with zero above-g samples. Its quadratic vertical acceleration RMS falls from **1.05248649969** to **0.183461043477 m/s²**. The new default realization peaks at **7.05810112987 m/s²**.

These are observed maxima, not universal acceleration caps or evidence of a vessel-response envelope. No acceleration clipping, tuning change, or high-frequency cutoff change was used. The unchanged PM rows are controls, not physical validation of the separate PM harmonic approximation.

The library has no startup or Live mode. This audit does not reclassify ocean-imu Live entry or rerun its filter quality gates. Position now includes integrated drift, and its consumer-side interpretation and attitude construction need the migration described in [the model notes](../jonswap-second-order.md).

## Reproduction and validation

- [Baseline CSV](reference-acceleration-baseline.csv), [corrected CSV](reference-acceleration-corrected.csv), and [corrected same-phases CSV](reference-acceleration-same-phases.csv).
- [Source hashes, baseline revision and commands](jonswap-audit-provenance.json).
- `make -C tests check` passes all eight test groups, including independent physical equations and trajectory/IMU consistency.
- The AddressSanitizer/UndefinedBehaviorSanitizer executable passes locally with `ASAN_OPTIONS=detect_leaks=0`; this environment prevents LeakSanitizer process inspection. CI retains the full sanitizer gate without that override.
- The data-sim executable compiles. CI regenerates the full simulation/plot/PDF artifacts; the dedicated acceleration audit is uploaded separately.

Pair angle-addition identities reduce transcendental evaluations from O(N²) to O(N), retaining every pair. The optimized and direct-trigonometry implementations agree to the 12-digit output precision in the eight-case short comparison. Full numerical evidence here uses the optimized implementation.
