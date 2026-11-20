## Purpose

Record the decisions about the architecture and the strategy of microkernel. Give the context, the alternatives, and the consequences.

## Structure

Each decision is a block with the sections `Context`, `Decision`, `Alternatives`, and `Consequences`. You can read the block alone. The blocks are in groups by category. The ADRs are newest first (git log order).

## Formatting Rules

- Heading: `## ADR-N (YYYY-MM-DD): Title`, where N is the next sequential number.
- Subsections: `### Context`, `### Decision`, `### Alternatives`, `### Consequences`.
- The ADRs are newest first (git log order). Put a new ADR at the top of the file, below the `<!-- [CAT] Category -->` comment of its category.
- To replace a decision, add a new ADR that supersedes it. Do not change the old ADR.

<!-- [CAT] Tooling -->

## ADR-6 (2026-11-20): Tune the clang tool configurations to the codebase

### Context

The new `.clang-tidy` and `.clang-format` must work with the existing code. The verification in the container found three problems: clang-tidy flags the linker symbols (`__bss_start`, `__text_start`, and similar) as reserved identifiers; the analyzer flags the reads of syscall out-params as uninitialized; and clang-tidy needs a compilation database that the configure step did not make.

### Decision

- Remove `bugprone-reserved-identifier` from `.clang-tidy`. The linker symbol names are intentional (see `kernel/linker.ld`).
- Keep the analyzer checks. Suppress the syscall out-param false positives at the call site with named `NOLINT` comments (see the [clang-tidy skill](skills/clang-tidy/SKILL.md)).
- Add `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` to the configure step in `scripts/env.sh`.

### Alternatives

- Disable the analyzer checks project-wide (loses real findings)
- Annotate the syscall wrappers for the analyzer (clang 14 does not use the annotations)

### Consequences

- The lint gate is clean and catches regressions.
- Each new read of a syscall out-param needs one `NOLINT` line.

<!-- [CAT] Documentation -->

## ADR-5 (2026-11-20): Write documentation in ASD-STE100 Simplified Technical English

### Context

The project documents (README, task plan) already use short sentences with approved words. The project has term lists: `technical_terms.txt` (technical nouns and verbs) and `allowed_phrases.txt` (multi-word technical terms).

### Decision

All technical documents (README.md and `docs/`) follow ASD-STE100 Issue 9. Writers use the term lists in the repository. Agents use the asd-ste100 skill when they write or check documentation.

### Alternatives

- Free prose (inconsistent vocabulary, harder to check)
- A full style guide with more rules than STE

### Consequences

- Documents use short sentences, active voice, and approved words.
- The term lists are the project-specific dictionary. Writers add new technical terms there.
- `docs/CONVENTIONS.md` records this rule for all writers.

<!-- [CAT] Process -->

## ADR-4 (2026-11-20): Keep the env.sh container workflow

### Context

The agent-scaffolding dev-environment module usually makes a Podman Compose setup with a `docker-compose.yml`. This project already has a working container workflow: the `Containerfile` makes the image `microkernel-dev`, and `scripts/env.sh` drives it with `podman run`.

### Decision

Keep the existing `Containerfile` and `scripts/env.sh`. The module gives only `scripts/enter-dev.sh`, which delegates to `env.sh shell`. The dev-environment skill documents this workflow.

### Alternatives

- Migrate to Podman Compose with a `dev` service (a second entry point for the same image)

### Consequences

- One entry point (`env.sh`) for configure, build, test, clean, and shell.
- The image build and the container flags stay in one script.

<!-- [CAT] Structure -->

## ADR-3 (2026-11-20): Move tests and test components into tests/

### Context

The test harness lived in `scripts/`, and the test applications `ipccli` and `sigtest` lived in `apps/`. The test files had no single root folder.

### Decision

Collect all test files under `tests/` at the repository root: `tests/test_runner.py`, `tests/ipccli/`, and `tests/sigtest/`. The `apps/` directory is gone. Test applications are Ring 3 programs. The build packs them into `boot.tar` as before.

### Alternatives

- Keep the test applications in `apps/` and move only the harness
- Keep the harness in `scripts/` next to the build helpers

### Consequences

- All test files are in one folder.
- The CMake `add_subdirectory` paths and `scripts/env.sh` use the new paths.
- The build and the test run give the same results as before the move.

<!-- [CAT] Licensing -->

## ADR-2 (2026-11-20): Use the MIT License

### Context

The project had no license file. Contributors and distributors need clear license terms.

### Decision

Add `LICENSE.md` with the MIT License. The copyright line is `Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>`. Each source file gets this line as a header comment, with an `SPDX-License-Identifier: MIT` line below it.

### Alternatives

- GPL-family licenses (strong copyleft, heavier compliance for distributors)
- Apache-2.0 (explicit patent grant, longer notice text)
- BSD-2/3-Clause (shorter, no trademark clause)

### Consequences

- Distributors must keep the copyright notice and the license text.
- Each new source file starts with the copyright header (see [CONVENTIONS.md](CONVENTIONS.md)).
- `docs/THIRD-PARTY-NOTICES.md` keeps the third-party attributions separate.

<!-- [CAT] Governance -->

## ADR-1 (2026-11-20): Adopt agent-scaffolding

### Context

The project must have governance, documentation, and agent instructions that work with all AI agents.

### Decision

Apply the agent-scaffolding agent skill 0.2.0 with the configuration in `.agent-scaffolding.yml`. The commit mode is AUTONOMOUS.

### Alternatives

- Documents and agent instructions that you write for each project
- Instructions for one platform only (for example only `CLAUDE.md` or only `.github/copilot-instructions.md`)

### Consequences

- `AGENTS.md` is the supreme authority for agent behavior.
- Procedures are only in `docs/skills/`.
- The state documents (`docs/TASKS.md`, `CHANGELOG.md`, and `docs/DECISIONS.md`) are up to date before each commit.
