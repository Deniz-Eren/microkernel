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

- Two-tier bidirectional code and POSIX.1-2024 traceability: `:posix:` field and
  calculated compliance matrix in `docs/60-verification/traceability.md`,
  bidirectional `ICD_`/`COMP_`/`VER_` coverage check in
  `config/sphinx/need_tag_check.py`, `WDR_0002`, `ICD_`/`COMP_` code tags across
  all active `kernel/`, `libc/`, `servers/`, and traceability tooling files, and
  governance rules in `AGENTS.md`.
- `<errno.h>` and lock-free TID-indexed thread-local `errno`
  (`libc/src/errno.c`) with the full POSIX.1-2024 `E*` constant set in
  `libc/include/sys/errno.h`.
- Standalone POSIX.1-2024 header compilation and namespace hygiene CTest gate
  (`posix.headers` via `tests/posix/test_headers.py`) and multithreaded `errno`
  verification in `tests/sigtest/main.c`.
- POSIX.1-2024 (Issue 8) Base and option-group software requirements across
  `LIBC`, `SIG`, `MEM`, `PROC`, `IPC`, and `DOCS` with canonical `9799919799`
  specification URLs, plus `ADR_0003`, `CNV_0004`, and `TSK_0003`.
- Real traceability domains (`BOOT`, `MEM`, `PROC`, `IPC`, `SIG`, `LIBC`, and
  `DOCS`) across all Sphinx-Needs layers, plus `BDR_0002`, `BDR_0003`
  (POSIX.1-2024 Base conformance target), `ADR_0002`, and `microkernel` tasks
  and roadmap records.
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

- Switched `authoring.modal_verb` in `.docs-traceability-setup.yml` from `shall`
  to `must` for ASD-STE100 Issue 9 compliance, revised all requirements in
  `docs/20-system/` to follow STE rules, expanded `technical_terms.txt` and
  `allowed_phrases.txt`, and made ASD-STE100 requirements compliance explicit in
  `AGENTS.md`, `WDR_0002`, and `CNV_0002`.
- Retargeted in-code `VER_` tags in `tests/test_runner.py`,
  `tools/traceability/check_needs_audit.py`, and `tests/traceability/` from
  `VER_POWER_*` to the real `BOOT`, `MEM`, `PROC`, `IPC`, `SIG`, `LIBC`, and
  `DOCS` verification items.
- The root `CMakeLists.txt` builds the documentation with
  `add_subdirectory(docs)` and enables CTest.
- The build container adds the `python3-venv` and `python3-pip` packages for the
  documentation venv.
- The headless QEMU test harness `tests/test_runner.py` carries the
  `VER_POWER_0005` traceability tag.

### Removed

- The `POWER` placeholder domain documents (`docs/*/power.md`) from the
  traceability model.

### Fixed

- Aligned secondary Ring 3 thread entry stacks in `sys_thread_reset` to satisfy
  the System V AMD64 `RSP % 16 == 8` C function entry invariant, preventing
  `#GP` faults on SSE `movaps` instructions in spawned worker threads.

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
