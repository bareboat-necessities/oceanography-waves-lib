# 1.3.0

- Add a separate underway RAO simulation for all four sailboat presets (28, 34, 42 and 50 ft): 5 kn along a heading oscillating ±3 degrees about north with a 120-second sine cycle.
- Sample incident waves along the moving track and apply the existing estimated response at instantaneous heading and signed encounter frequency. Include turn acceleration, response modulation, rotating-frame terms and full yaw-pitch-roll body rates in the reference/IMU data.
- Generate separate `sim-data-files-vessel-rao-<size>ft-5kn.zip` files with the same flat CSV filenames, columns, clocks and incident spectra as the existing datasets. Report total CG travel and velocity, including forward motion.
- Audit all 80 additional full records in CI and require the four new ZIPs and four audit CSVs in the release manifest. Test all 160 stationary/moving vessel cases and all nine archive layouts.
- Preserve the original surface and stationary vessel datasets and chart/PDF assets. Document the quasi-steady model's scope: existing estimated RAOs with encounter corrections, without hull-specific forward-speed hydrodynamics or propulsion/steering dynamics.

Tag `v1.3.0` is published from the exact main commit after numerical tests, full simulation audits, all five stationary/surface chart jobs and all nine PDF builds pass. All 32 data, chart, PDF and audit assets plus `SHA256SUMS` are verified before publication. Previous releases remain unchanged.
