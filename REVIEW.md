# Repository Review (2026-03-30)

## Overall score: **8/10**

## What is strong
- Clear project scope and structure, with deterministic and spectral models documented in the README.
- Good practical build/run guidance and multiple usage paths (tests, simulation generation, plotting).
- Tests cover key numerical invariants and error handling for major models.
- CI includes sanitizer-based test runs and end-to-end artifact generation.

## Main gaps preventing a higher score
- Testing is executable-based and focused on selected paths; there is no coverage reporting or broader property-based/regression suite.
- APIs are mostly header-only without generated API reference docs (e.g., Doxygen).
- Build system is split across folder-local Makefiles rather than a top-level portable build (e.g., CMake package/export), which can limit downstream integration.
- CI workflow is ambitious but fairly heavy, with several external moving parts that may increase maintenance burden.

## Suggested next steps
1. Add top-level CMake project with install/export targets for consumers.
2. Add a small benchmark suite and numerical regression baselines.
3. Add API docs generation and publish with CI artifacts.
4. Track line/branch test coverage and add minimum thresholds.
