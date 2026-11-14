# 1.3.1

- Fix six-significant-digit timestamps, which collapsed 200 Hz samples onto a 0.01 s grid at 1,000 s. Serialize time independently with 17 significant digits for exact double round trips in every dataset.
- Generate time from integer sample indices divided by 200, eliminating accumulated timestep error. Use a double clock for the Fenton surface tracker and the same sample schedule for stationary and moving vessel responses.
- Retain Fenton's initial two-sample skip and inclusive final endpoint. Correct full-duration Fenton records to 239,999 samples (0.010–1200.000 s), replacing the drifted 239,795-row clock. Other records contain 240,000 samples (0–1199.995 s).
- Regenerate all nine simulation ZIPs through the release pipeline. Reject invalid CSV clocks before packaging and verify exact timestamps and sample counts inside every ZIP in CI. ZIP members preserve CSV bytes.
- Add full-duration writer/tracker regression tests and retain all vessel compatibility and physics checks.

Version 1.3.1 is published from the exact successful main commit after all existing tests, simulations, audits, charts and PDFs pass. Older releases remain immutable; their rounded timestamps and Fenton clock drift require regeneration, not relabeling.
