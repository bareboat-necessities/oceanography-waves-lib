# Additional 28-foot sailboat simulation

`waves_sim --vessel-rao` produces vessel motion and a CG-mounted IMU alongside
the existing wave-particle simulation. The default preset is an **estimated
28 ft (8.53 m) fin-keel displacement sailboat**, upright, at zero forward speed,
with fixed heading along world +x. It is a configurable analytical surrogate;
the coefficients are not measured RAOs for a particular production boat.

## Preset

| Parameter | Value |
|---|---:|
| Waterline length | 7.0 m |
| Beam | 2.9 m |
| Draft including fin keel | 1.5 m |
| Heave natural period / damping ratio | 2.4 s / 0.45 |
| Pitch natural period / damping ratio | 2.8 s / 0.35 |
| Roll natural period / damping ratio | 3.5 s / 0.22 |
| Surge / sway response time | 0.7 s / 1.0 s |
| Forward speed / mean heel / heading | 0 m/s / 0 degrees / 0 degrees |
| Response and IMU origin | center of gravity |

These are engineering choices for a moderately heavy small cruising sailboat
(roughly 3–4 tonnes), not a fit to a hull calculation. Mass and ballast are not
independent inputs: their approximate effects are represented by the selected
natural periods and damping. In particular the fin keel is represented through
the roll period, roll damping and sway response; keel loads are not solved.
All parameters used in the equations are in `VesselRao::Parameters`.

## Response equations and phase

Each incident component uses

\[
\eta=a\cos(k(x\cos\beta+y\sin\beta)-\omega t+\phi).
\]

Here beta is the wave **propagation** direction relative to the vessel heading.
Beta=0 is a following wave and beta=pi is a head wave. The zero-speed symmetric
preset has the same gains in either direction, with the appropriate signs.
Translations are in metres per metre of elevation amplitude, rotations in
radians per metre. The component response is
`Re[H * a * exp(i*phi - i*omega*t)]`; positive complex phase is a time lag.

For length L, beam B, draft D, define the smooth waterplane average

\[
F=\exp[-((kL\cos\beta)^2+(kB\sin\beta)^2)/24],\quad
P=F\exp(-0.35kD),
\]

and the damped oscillator and nonresonant horizontal filter

\[
G_j={1\over 1-r_j^2-2i\zeta_jr_j},\quad
r_j={\omega T_j\over 2\pi},\qquad
Q_j={1\over(1-i\omega\tau_j)^2}.
\]

The Gaussian footprint has the second spatial moment of a rectangular
waterplane. It is a smooth approximation to wave cancellation across the hull,
not a diffraction solution. The displacement RAOs are

\[
\begin{aligned}
H_{surge}&=i\cos\beta\,P Q_{surge},&
H_{sway}&=i\sin\beta\,P Q_{sway},&
H_{heave}&=F G_{heave},\\
H_{roll}&=ik\sin\beta\,F G_{roll},&
H_{pitch}&=-ik\cos\beta\,F G_{pitch},&
H_{yaw}&=0.
\end{aligned}
\]

The five active responses approach surface translation and slope-following in
long waves, and vanish in short waves. Yaw is intentionally constrained by the
fixed-heading assumption; this is not an unconstrained six-DOF maneuvering model.
The heave/pitch/roll response includes resonant amplification and phase lag.
There is no acceleration clamp or post-generation scaling to force peaks below g.

The displacement-RAO definition and origin conventions follow
[Orcina's displacement RAO documentation](https://www.orcina.com/webhelp/OrcaFlex/Content/html/Vesseltypes,RAOs.htm).
The long/short-wave checks and their physical interpretation are described in
[Orcina's RAO quality checks](https://www.orcina.com/webhelp/OrcaFlex/Content/html/Vesseltheory,RAOqualitychecks.htm).
Those references support the conventions and limiting tests, **not this
sailboat's chosen numerical coefficients**.

## Incident waves and existing records

The original surface/particle mode is unchanged. The additional vessel mode
uses the same scenario inputs, seed, frequency band and directional distribution.

* JONSWAP uses the existing model's actual first-order component amplitudes,
  frequencies, directions and phases. It does not treat second-order particle
  advection or Stokes drift as incident elevation forcing.
* PM uses the actual first-order components after the existing PM model's
  amplitude normalization. Its higher-order particle harmonics are excluded.
* Fenton uses its five solved Eulerian elevation harmonics at the original
  initial x, with each harmonic's actual frequency, wavenumber and phase.
* Gerstner inverts the existing surface map at a fixed horizontal location,
  then takes 64 Fourier harmonics from 2048 samples of the incident profile.
* Cnoidal projects the existing implemented profile in the same way, preserving
  its amplitude inputs and actual repeat period. The legacy Jacobi helper has
  an extra phase halving, so that period is `8K/omega`, rather than mathematical
  cn's `4K/omega` or the nominal period encoded in the filename. This addition
  preserves that existing behavior instead of changing the original dataset.

The mean elevation datum of regular profiles is omitted from oscillatory vessel
displacement. Applying the linear response to bound regular-wave harmonics is
an additional approximation, not a second-order vessel-force calculation.
Spectrum CSVs describe the **incident sea** and are identical between archives.
They are not vessel-motion spectra. Wave height, period-derived length, phase,
and direction in filenames continue to identify the incident case.

## CSV and archive compatibility

The original `sim-data-files.zip` is still generated. The additional archive is
**`sim-data-files-vessel-rao-28ft.zip`**. Its members have exactly the same flat
names, CSV column order, units and scenario clocks as the original archive.
All 20 `wave_data_*.csv` files contain vessel results. Eight spectrum CSVs and
ancillary Fenton plotting CSVs retain their incident-wave meaning. No metadata
files or directory prefix are inserted into the replacement ZIP.

The spectral and Gerstner/Cnoidal records retain 240,000 samples at nominal
200 Hz over 20 minutes. Fenton retains its existing float clock and initial
sample omission, including the existing difference in its row count. Both
modes use the same CSV writer.

* `disp_*`, `vel_*`, `acc_*`: vessel CG motion in world axes; acceleration excludes gravity.
* `acc_b*`: body-frame specific force, `C_wb * (a_world + [0,0,g])`.
* `gyro_*`: body angular velocity, radians/second, from the full attitude derivative.
* `roll_deg,pitch_deg,yaw_deg`: ZYX Euler angles in degrees.
* `q_wb_zu_*`: unit quaternion rotating world z-up vectors into body axes, wxyz order.

Body axes are x forward, y port, z up. The body-to-world rotation is
`Rz(heading) Ry(pitch) Rx(roll)`. Positive roll raises the port side; positive
pitch lowers the bow. With fixed heading, body rate is
`[roll_dot, pitch_dot*cos(roll), -pitch_dot*sin(roll)]`, so body gyro z need not
be zero even though Euler yaw is fixed. At rest the accelerometer reports +g
along body z. No surface-normal attitude is substituted for vessel attitude.

Generate and package locally:

```sh
cd data-sim
bash gen_sim_data.sh
# Optional: generate the same auxiliary Fenton plotting CSVs as CI.
make -C ../plots
(cd ../plots && ./fenton_plots)
mv ../plots/*.csv .
python3 package_sim_data.py
```

For vessel data alone: `make -C data-sim`, then
`data-sim/waves_sim --vessel-rao --output-dir path/to/vessel-rao-28ft`.
The executable also accepts a case index 0–3 and `--duration SECONDS` for shorter
runs. Without `--output-dir`, vessel files go in `vessel-rao-28ft/` under the
current directory, preventing default output from overwriting particle files.

CI uploads the separate replacement ZIP on PRs. After all tests, chart jobs and PDF builds pass on main, the complete versioned
release includes all five data archives, SVG/PGF chart archives and PDFs. Release
1.2.2 is tagged `v1.2.2`; the RAO chart document is
`wave_sim_charts_vessel_rao_28ft.pdf`. The packaging step
rejects missing/extra wave files, schema changes, or changed incident spectra.
`python3 tests/vessel_archive.py` checks all 80 vessel cases, exact time-column parity,
finite data, IMU frame recovery, archive names and auxiliary-file preservation.

## Scope

This is a steady-state linear response about an upright mean pose. There is no
start-up transient, sailing speed, sail/wind loading, mean heel, wave yaw,
surge/sway drift, mooring, nonlinear restoring, green water, slamming, breaking,
capsize or sensor lever arm. Exact rotation of the simulated attitude keeps IMU
channels mutually consistent but does not make the hydrodynamic RAOs nonlinear.
The Hs=8.5 m case is an extrapolated stress test for a boat this size, not a
prediction of survivable or realistic storm motion. Replace this preset with
measured or computed hull-specific RAOs for quantitative vessel predictions.

## Reference validation

All 20 full vessel records were generated and audited, including finite-value
checks and exact expected row counts. The spectral records each contain
240,000 samples. Full results and SHA-256 hashes of the generated CSV bytes are
in [vessel-rao-reference-audit.csv](validation/vessel-rao-reference-audit.csv).

| Incident spectrum | Hs (m) | Peak CG acceleration (m/s²) | Maximum absolute roll | Maximum absolute pitch |
|---|---:|---:|---:|---:|
| jonswap | 0.27 | 0.923 | 3.57° | 4.90° |
| jonswap | 1.5 | 2.713 | 12.93° | 17.05° |
| jonswap | 4 | 4.347 | 18.18° | 21.60° |
| jonswap | 8.5 | 5.913 | 24.56° | 29.45° |
| pmstokes | 0.27 | 0.893 | 3.51° | 4.50° |
| pmstokes | 1.5 | 2.991 | 15.05° | 18.69° |
| pmstokes | 4 | 5.040 | 23.32° | 25.97° |
| pmstokes | 8.5 | 6.938 | 30.11° | 35.49° |

No generated vessel record exceeded g in CG acceleration. This is an observed
result, not a limiter; the large rotations in the extreme cases also exceed
the small-motion regime in which these estimated RAOs are most credible.
The CG acceleration excludes gravity; body accelerometer specific force includes it.

Reproduce the audit after a full run:

```sh
python3 data-sim/audit_vessel_data.py data-sim/vessel-rao-28ft > vessel-rao-reference-audit.csv
```

[Source provenance](validation/vessel-rao-provenance.json) records the generator
and model hashes. In this execution environment, some large native output files
were shortened during process/file handoff even though immediate readback in the
generator was complete. For this local evidence, complete CSV bytes were streamed
from the generator to the parent process before exit, then saved and independently
audited with the script above. This transport-only workaround changes no samples
and is not needed by the model API. CI audits its saved files before publishing.

## Additional 34 ft, 42 ft, and 50 ft presets

`VesselRao::sailboat(length_feet)` selects 28, 34, 42, or 50 ft. The 28 ft
preset preserves the original parameters. The larger presets are geometrically
similar fin-keel surrogates, not measured responses of particular production
boats. Set `R = length_feet / 28`: waterline length, beam, and keel draft scale
by `R`; natural periods and surge/sway time constants scale by `sqrt(R)`.
Dimensionless damping ratios, gravity, and fixed mean heading stay unchanged.
This follows [Froude scaling of vessel response data](https://www.orcina.com/webhelp/OrcaFlex/Content/html/Vesseldata.htm).
At corresponding scaled frequencies, translation RAOs remain dimensionless
and rotational RAOs in rad/m scale by `1/R`. The tests check this relation for
all six response components. The incident wave spectrum itself is not scaled.

| Nominal LOA | Waterline (m) | Beam (m) | Draft (m) | Heave period (s) | Pitch period (s) | Roll period (s) |
|---|---:|---:|---:|---:|---:|---:|
| 28 ft | 7.000 | 2.900 | 1.500 | 2.400 | 2.800 | 3.500 |
| 34 ft | 8.500 | 3.521 | 1.821 | 2.645 | 3.085 | 3.857 |
| 42 ft | 10.500 | 4.350 | 2.250 | 2.939 | 3.429 | 4.287 |
| 50 ft | 12.500 | 5.179 | 2.679 | 3.207 | 3.742 | 4.677 |

Generate one preset with `./waves_sim --vessel-length-ft 42`, or run
`bash gen_sim_data.sh` from `data-sim` to generate the surface dataset and all
four vessels. `--vessel-rao` remains an alias for the original 28 ft response;
`--vessel-length-ft` also enables vessel mode. Unsupported lengths are rejected.
The default output directory is `vessel-rao-<length>ft`.

Packaging requires all four vessel directories, compares their schemas and
incident spectra with the surface files, and produces:

- `sim-data-files.zip`
- `sim-data-files-vessel-rao-28ft.zip`
- `sim-data-files-vessel-rao-34ft.zip`
- `sim-data-files-vessel-rao-42ft.zip`
- `sim-data-files-vessel-rao-50ft.zip`

All five archives have identical flat CSV member names. Auxiliary incident-wave
plotting tables are copied unchanged. CI audits each full set of 20 vessel
records, produces 20 motion/IMU charts per hull, and builds separate
`plot-files-vessel-rao-<length>ft.zip` and
`wave_sim_charts_vessel_rao_<length>ft.pdf` outputs. New sizes use distinct PGF
prefixes so their figures cannot overwrite the existing 28 ft figures.

Geometric similarity does not capture hull-specific beam/length ratios,
ballast, loading, roll damping, sailing speed, or nonlinear slamming. In
particular, the 8.5 m incident seas remain extrapolative stress tests. These
presets must not be interpreted as validated heavy-weather predictions.
