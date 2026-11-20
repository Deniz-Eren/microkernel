## Purpose

Record the important changes of microkernel. The format follows [Keep a
Changelog](https://keepachangelog.com).

## Structure

- The `## [Unreleased]` section is at the top. Below it, the releases are newest
  first (git log order).
- Category headings of level 3 in each section: `### Added`, `### Changed`, `###
  Deprecated`, `### Removed`, `### Fixed`, and `### Security`. Put a heading
  into the file only when the section has content for it.

## Formatting Rules

- Release heading: `## [x.y.z] - YYYY-MM-DD`.
- The entries in each category are newest first (git log order).
- Write each entry as a complete sentence with a final period.

## [Unreleased]

### Added

- The docs-as-code traceability model with the docs-traceability-setup agent
  skill 0.1.0. The configuration is in `.docs-traceability-setup.yml`.
- The Sphinx documentation in `docs/` with MyST-Parser and Sphinx-Needs, built
  by CMake with warnings as errors and checked by CTest (unit tests, HTML build,
  needs export, model audit).
- The traceability example items for the `POWER` domain, the decision records,
  and the project records in `docs/records/`.
- The EditorConfig file `.editorconfig` with the file header notice lines, and
  the Python dependency manifest `requirements.txt`.

### Changed

- The root `CMakeLists.txt` builds the documentation with
  `add_subdirectory(docs)` and enables CTest.
- The build container adds the `python3-venv` and `python3-pip` packages for the
  documentation venv.
- The headless QEMU test harness `tests/test_runner.py` carries the
  `VER_POWER_0005` traceability tag.

### Added

- Scaffolding with the agent-scaffolding agent skill 0.2.0. The configuration is
  in `.agent-scaffolding.yml`.
- The MIT License in `LICENSE.md`, with copyright headers and SPDX identifiers
  in the source files.
- Governance and state documents: `AGENTS.md`, `docs/VISION.md`,
  `docs/DECISIONS.md`, `docs/CONVENTIONS.md`, `docs/THIRD-PARTY-NOTICES.md`, and
  this changelog.
- Agent skills in `docs/skills/`, with the Conventional Commits, SemVer, and
  Agent Skills specifications as references.
- A containerized development environment wrapper `scripts/enter-dev.sh`, and
  build and formatter agent scripts with their agent definitions in
  `docs/agents/`.
- Clang tool configurations `.clang-format` and `.clang-tidy`, verified in the
  build container.

### Changed

- The task plan moved from `TASKS.md` at the repository root to `docs/TASKS.md`.
- The test harness and the test applications moved to `tests/`:
  `tests/test_runner.py`, `tests/ipccli/`, and `tests/sigtest/`.
