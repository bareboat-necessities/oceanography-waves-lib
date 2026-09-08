# 1.2.3

- Add applied RAO gain, phase and heading-response charts for all four sailboat presets: 28, 34, 42 and 50 ft.
- Sample the production C++ transfer function directly, showing all six motion components, translation and rotation units, relative wave propagation headings, phase convention and constrained yaw.
- Include the new PNG/SVG/PGF charts in each vessel chart archive and PDF. Add viewable 28 ft previews and standalone regeneration commands in the RAO documentation.
- Preserve all five interchangeable simulation datasets and the existing vessel response equations and numerical quality gates.

The charts show the deep-water transfer-function slice, not sea-state-weighted motion spectra. The vessel presets remain estimated zero-speed analytical surrogates, not measured hull RAOs.

Tag `v1.2.3` is published from the exact main commit after numerical tests, simulation audits, all five chart jobs and all nine PDF builds pass. All 24 data, chart, PDF and audit assets plus `SHA256SUMS` are verified before publication. Previous releases remain unchanged.
