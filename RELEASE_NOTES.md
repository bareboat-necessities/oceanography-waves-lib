# 1.2.1

- Fix the PDF build by removing unused PlantUML downloads, including the CTAN mirror request that failed certificate verification.
- Add vessel RAO charts for all five wave families and all four incident heights, covering CG displacement/velocity/acceleration, accelerometer specific force, body angular velocity, and Euler angles.
- Include `wave_sim_charts_vessel_rao_28ft.pdf` and `plot-files-vessel-rao-28ft.zip` (SVG and PGF) alongside the surface-model charts.
- Preserve the interchangeable surface and vessel CSV archive filenames and schemas.
- Publish this version from the exact main commit that passed tests, full data generation/audits, both chart jobs, and all six PDF builds. Upload all assets and verify their names, sizes and available digests before making the release public.

Download `sim-data-files-vessel-rao-28ft.zip` to use the approximate stationary 28-foot fin-keel sailboat response. Download `sim-data-files.zip` for the surface/particle model. The vessel RAO is an estimated analytical preset, not measured hull data; extreme sea states are stress tests.

`SHA256SUMS` covers the data archives, chart archives, PDFs and reference audits.
