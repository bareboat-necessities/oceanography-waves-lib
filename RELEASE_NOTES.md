# 1.4.0

- Add a separate southbound 5 kn simulation preset for the 28, 34, 42 and 50 ft vessels: `waves_sim --vessel-moving-south --vessel-length-ft <size>`.
- Use a 20-second steering cycle for both northbound and southbound presets, retaining the +/-3° amplitude and 5 kn speed. Heading is `3° sin(2πt / 20 s)` northbound and `180° + 3° sin(2πt / 20 s)` southbound. The maximum yaw rate is 0.94248 deg/s. Older northbound releases retain their original 120-second cycle.
- The unchanged incident waves approach the bow in the southbound preset: regular waves are head seas and directional spectra are centered on bow-quartering seas.
- Add `sim-data-files-vessel-rao-<size>ft-5kn-south.zip` for each hull, with the same flat CSV members, schemas, incident spectra, 200 Hz clocks and 20-minute duration as the existing archives. Northbound `-5kn.zip` archives retain their names and are regenerated with the new 20-second steering cycle. Surface and stationary datasets retain their calculations and names.
- Reuse the existing moving-vessel RAO and exact steering trajectory with a southward mean heading. Apply the heading consistently to travel, encounter frequencies, vessel response, attitude, accelerometer and gyro references.
- Generate and audit all 240 vessel records, validate clocks in all thirteen ZIPs, and require the four southbound ZIPs and their four audit CSVs in the release manifest. Add head-sea encounter, southbound kinematics, heading and archive compatibility regression coverage.

Version 1.4.0 is published from the exact successful main commit after tests, simulations, audits, charts and PDFs pass. Older releases remain immutable. The existing charts and PDFs continue to describe the stationary cases.
