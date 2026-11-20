## Purpose

Record the important changes of microkernel. The format follows [Keep a Changelog](https://keepachangelog.com).

## Structure

- The `## [Unreleased]` section is at the top. Below it, the releases are newest first (git log order).
- Category headings of level 3 in each section: `### Added`, `### Changed`, `### Deprecated`, `### Removed`, `### Fixed`, and `### Security`. Put a heading into the file only when the section has content for it.

## Formatting Rules

- Release heading: `## [x.y.z] - YYYY-MM-DD`.
- The entries in each category are newest first (git log order).
- Write each entry as a complete sentence with a final period.

## [Unreleased]

### Added

- Scaffolding with the agent-scaffolding agent skill 0.2.0. The configuration is in `.agent-scaffolding.yml`.
- The MIT License in `LICENSE.md`, with copyright headers and SPDX identifiers in the source files.
- Governance and state documents: `AGENTS.md`, `docs/VISION.md`, `docs/DECISIONS.md`, `docs/CONVENTIONS.md`, `docs/THIRD-PARTY-NOTICES.md`, and this changelog.
- Agent skills in `docs/skills/`, with the Conventional Commits, SemVer, and Agent Skills specifications as references.
- A containerized development environment wrapper `scripts/enter-dev.sh`, and build and formatter agent scripts with their agent definitions in `docs/agents/`.
- Clang tool configurations `.clang-format` and `.clang-tidy`, verified in the build container.

### Changed

- The task plan moved from `TASKS.md` at the repository root to `docs/TASKS.md`.
- The test harness and the test applications moved to `tests/`: `tests/test_runner.py`, `tests/ipccli/`, and `tests/sigtest/`.
