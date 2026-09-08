# 1.2.2

- Add 34 ft, 42 ft and 50 ft fin-keel sailboat RAO presets alongside the original 28 ft preset and surface/particle model.
- Provide separate `sim-data-files-vessel-rao-34ft.zip`, `sim-data-files-vessel-rao-42ft.zip` and `sim-data-files-vessel-rao-50ft.zip`. All five simulation archives retain identical flat CSV filenames, columns, units and scenario clocks for drop-in replacement.
- Include separate SVG/PGF chart ZIPs and 21-page vessel chart PDFs for all four boat sizes, covering all five wave families and four incident heights.
- Fully audit all 80 vessel records and publish per-size CSV audits. Include all nine PDFs and `SHA256SUMS` for the 24 data, chart, PDF and audit assets.
- Preserve the original 28 ft response. Larger presets use geometric/Froude scaling: dimensions scale with vessel length and response periods/time constants with its square root. These are approximate stationary fin-keel surrogates, not measured hull RAOs; extreme seas remain stress tests.

Use `./waves_sim --vessel-length-ft 28|34|42|50` to select a preset (choose one value). `--vessel-rao` still selects the original 28 ft default. The standard generation and packaging workflow produces all variants.

This release is tagged `v1.2.2` at the main commit that passed numerical tests, full simulation audits, all five chart jobs and all nine PDF builds. Assets are uploaded to a draft and checked for completeness, size and SHA-256 digest before publication. Release 1.2.1 remains unchanged.
