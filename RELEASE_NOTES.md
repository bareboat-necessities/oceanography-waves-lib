# 1.4.0

- Add a separate southbound 5 kn simulation preset for the 28, 34, 42 and 50 ft vessels: `waves_sim --vessel-moving-south --vessel-length-ft <size>`.
- Keep the existing steering amplitude and period: heading is `180° + 3° sin(2πt / 120 s)`, with 5 kn along the instantaneous heading. The unchanged incident waves now approach the bow: regular waves are head seas and directional spectra are centered on bow-quartering seas.
- Add `sim-data-files-vessel-rao-<size>ft-5kn-south.zip` for each hull, with the same flat CSV members, schemas, incident spectra, 200 Hz clocks and 20-minute duration as the existing archives. The original northbound `-5kn.zip` and stationary datasets retain their calculations and names.
- Reuse the existing moving-vessel RAO and exact steering trajectory with a southward mean heading. Apply the heading consistently to travel, encounter frequencies, vessel response, attitude, accelerometer and gyro references.
- Generate and audit all 240 vessel records, validate clocks in all thirteen ZIPs, and require the four southbound ZIPs and their four audit CSVs in the release manifest. Add head-sea encounter, southbound kinematics, heading and archive compatibility regression coverage.

Version 1.4.0 is published from the exact successful main commit after tests, simulations, audits, charts and PDFs pass. Older releases remain immutable. The existing charts and PDFs continue to describe the stationary cases.
