## Purpose

Record the decisions about the architecture and the strategy of microkernel.
Give the context, the alternatives, and the consequences.

## Structure

Each decision is a block with the sections `Context`, `Decision`,
`Alternatives`, and `Consequences`. You can read the block alone. The blocks are
in groups by category. The ADRs are newest first (git log order).

## Formatting Rules

- Heading: `## ADR-N (YYYY-MM-DD): Title`, where N is the next sequential
  number.
- Subsections: `### Context`, `### Decision`, `### Alternatives`, `###
  Consequences`.
- The ADRs are newest first (git log order). Put a new ADR at the top of the
  file, below the `<!-- [CAT] Category -->` comment of its category.
- To replace a decision, add a new ADR that supersedes it. Do not change the old
  ADR.

<!-- [CAT] Tooling -->

## ADR-10 (2026-11-21): Implement lock-free TID-indexed thread-local errno

### Context

`SWR_LIBC_0003` requires `<errno.h>` to expose a thread-local `errno` lvalue
that stays isolated across Ring 3 threads and remains async-signal-safe without
heap allocation or mutex locks. The kernel does not yet program per-thread
`FS.base` TLS descriptors.

### Decision

Implement `__errno_location()` in `libc/src/errno.c` using a fixed 64-slot
atomic table keyed by the calling thread's kernel TID (`ThreadGetCtx(0, &ti)`).
Each thread claims its own slot via `__atomic_compare_exchange_n`, starts with
`errno == 0`, and never allocates heap memory or acquires locks. Replace the
backing storage with `FS`-relative TLS when `pthread` TLS support lands without
changing the `<errno.h>` ABI.

### Alternatives

- Single global `int errno` (races between concurrent Ring 3 threads such as
  `ipcserv` workers and `sigtest`)
- Add kernel `FS.base` MSR management in this task (larger kernel scope outside
  `TSK_0001`)

### Consequences

- Concurrent Ring 3 threads have isolated `errno` values today and pass QEMU
  multithreaded verification.
- `__errno_location()` is async-signal-safe and requires no `malloc` or locks.
- Processes with more than 64 distinct thread IDs fall back to a shared slot
  until `FS.base` TLS lands.

## ADR-9 (2026-11-21): Align microkernel requirements to POSIX.1-2024 Base

### Context

The initial `microkernel` traceability domains describe the current milestones
1–4 implementation, but an audit against IEEE Std 1003.1-2024 (Issue 8) using
the `posix-issue8-os-dev` skill identified concrete gaps:
`libc/include/unistd.h` leaks `<sys/microkernel.h>`, `<errno.h>` and
thread-local `errno` are absent, `<sys/signal.h>` exposes non-POSIX
`sigaction`/`kill` signatures, `sys_sigaction` allows catching
`SIGKILL`/`SIGSTOP`, and mandatory Issue 8 Base groups (`pthread_*`, `sem_*`,
clocks/timers, `mmap`, `aio_*`) had no normative specs.

### Decision

Refine the requirements across `LIBC`, `SIG`, `MEM`, `PROC`, `IPC`, and `DOCS`
with atomic `draft` POSIX.1-2024 Base and option-group software requirements
(`SWR_LIBC_0002`–`0005`, `SWR_SIG_0003`–`0004`, `SWR_MEM_0004`–`0005`,
`SWR_PROC_0004`–`0006`, `SWR_IPC_0004`–`0005`, `SWR_DOCS_0003`), each citing
canonical Open Group publication `9799919799` URLs. Record the layered
microkernel strategy in `docs/30-architecture/adr/ADR_0003.md` and standing
header/citation rules in `CNV_0004`.

### Alternatives

- Keep POSIX alignment only as prose notes in `ROADMAP.md` without item-level
  traceability
- Mark unimplemented POSIX requirements as `active` before tests exist (breaks
  the Sphinx-Needs verification gate)

### Consequences

- Every POSIX.1-2024 Base requirement is tracked in the Sphinx-Needs model as
  `draft` and transitions to `active` as its implementation and automated
  verification gate land.
- Optional POSIX groups (`SHM`, `SPN`, `ML`, `MLR`, `MSG`, `PS`) stay separated
  from mandatory Base requirements.

## ADR-8 (2026-11-21): Replace POWER placeholder with real microkernel domains

### Context

ADR-7 adopted the Sphinx-Needs traceability model with the skill's default
`POWER` placeholder domain. Those sample power-supply items do not describe
`microkernel`, and tagging `tests/test_runner.py` against `SYS_POWER_0001`
produces misleading traceability. The next phase aligns `microkernel`
requirements and `libc` to POSIX.1-2024 (Issue 8) with the `posix-issue8-os-dev`
skill.

### Decision

Remove the `POWER` placeholder files and configure seven real domains in
`.docs-traceability-setup.yml`: `BOOT`, `MEM`, `PROC`, `IPC`, `SIG`, `LIBC`, and
`DOCS`. Populate all traceability layers (BDR, WDR, ADR, SYS, SWR, ARC, ICD,
COMP, VER, tasks, and roadmap) from the implemented kernel, `libc`, servers,
tests, and traceability tooling, with `BDR_0003` and draft `SWR_LIBC_0002`
prepared for POSIX.1-2024 Base alignment. Retarget in-code `VER_` tags in
`tests/test_runner.py`, `tools/traceability/check_needs_audit.py`, and
`tests/traceability/` to the real domains.

### Alternatives

- Retire the `POWER` items as `obsolete` and keep their files in the repository
- Use one monolithic `KERNEL` domain for all operating system subsystems

### Consequences

- Every active requirement, design item, and verification tag reflects real
  `microkernel` subsystems.
- Domain boundaries (`BOOT`, `MEM`, `PROC`, `IPC`, `SIG`, `LIBC`, `DOCS`) map
  cleanly to upcoming POSIX.1-2024 Base conformance work.

## ADR-7 (2026-11-20): Adopt the Sphinx traceability documentation model

### Context

The project records requirements, architecture, and verification only in prose
documents. There is no calculated traceability between requirements, design,
code, and tests. The milestone plan demands a test with exit code 0 for each
milestone, but no model connects the tests to normative items.

### Decision

Apply the docs-traceability-setup agent skill 0.1.0. The configuration is in
`.docs-traceability-setup.yml`: item IDs use the `number` style with 4 digits,
the primary markup is MyST, the initial domain is the schema default `POWER`
with the template example items, and the build connects through
`add_subdirectory(docs)` with CTest. The default build runs Sphinx with warnings
as errors and CTest runs the unit tests, the HTML build, the needs export, and
the model audit.

Two configuration decisions belong to this adoption:

- The pre-existing governance documents (`ARCHITECTURE.md`, `CONVENTIONS.md`,
  `DECISIONS.md`, `TASKS.md`, `VISION.md`, `THIRD-PARTY-NOTICES.md`, `agents/`,
  `skills/`) stay out of the Sphinx build and the tag scan through
  `model.code_tag_exclude`. They keep their own format and are read in the
  repository.
- The build container gets the `python3-venv` and `python3-pip` packages,
  because the docs venv needs `ensurepip`. The container workflow configures
  with `-DDOCS_AUTO_INSTALL=ON`, so the docs venv is created inside
  `build/docs/venv`. A fresh clone without this flag needs a local `.venv` (see
  `docs/README.md`).

### Alternatives

- Manual traceability matrices in Markdown tables or spreadsheets
- An external requirements management tool outside Git

### Consequences

- Each item is written one time with a permanent ID. All indexes and coverage
  views are calculated.
- Broken links, missing verification items, and invalid code tags stop the
  build.
- New human-authored Python, CMake, shell, and C/C++ files need a Doxygen header
  block with `@file`, `@brief`, and `@copyright` (see the conventions in
  `docs/records/CONVENTIONS.md`).
- The example `POWER` domain items are placeholders. Writers replace them with
  real kernel requirements and retire the samples.

## ADR-6 (2026-11-20): Tune the clang tool configurations to the codebase

### Context

The new `.clang-tidy` and `.clang-format` must work with the existing code. The
verification in the container found three problems: clang-tidy flags the linker
symbols (`__bss_start`, `__text_start`, and similar) as reserved identifiers;
the analyzer flags the reads of syscall out-params as uninitialized; and
clang-tidy needs a compilation database that the configure step did not make.

### Decision

- Remove `bugprone-reserved-identifier` from `.clang-tidy`. The linker symbol
  names are intentional (see `kernel/linker.ld`).
- Keep the analyzer checks. Suppress the syscall out-param false positives at
  the call site with named `NOLINT` comments (see the [clang-tidy
  skill](skills/clang-tidy/SKILL.md)).
- Add `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` to the configure step in
  `scripts/env.sh`.

### Alternatives

- Disable the analyzer checks project-wide (loses real findings)
- Annotate the syscall wrappers for the analyzer (clang 14 does not use the
  annotations)

### Consequences

- The lint gate is clean and catches regressions.
- Each new read of a syscall out-param needs one `NOLINT` line.

<!-- [CAT] Documentation -->

## ADR-5 (2026-11-20): Write documentation in ASD-STE100 English

### Context

The project documents (README, task plan) already use short sentences with
approved words. The project has term lists: `technical_terms.txt` (technical
nouns and verbs) and `allowed_phrases.txt` (multi-word technical terms).

### Decision

All technical documents (README.md and `docs/`) follow ASD-STE100 Issue 9.
Writers use the term lists in the repository. Agents use the asd-ste100 skill
when they write or check documentation.

### Alternatives

- Free prose (inconsistent vocabulary, harder to check)
- A full style guide with more rules than STE

### Consequences

- Documents use short sentences, active voice, and approved words.
- The term lists are the project-specific dictionary. Writers add new technical
  terms there.
- `docs/CONVENTIONS.md` records this rule for all writers.

<!-- [CAT] Process -->

## ADR-4 (2026-11-20): Keep the env.sh container workflow

### Context

The agent-scaffolding dev-environment module usually makes a Podman Compose
setup with a `docker-compose.yml`. This project already has a working container
workflow: the `Containerfile` makes the image `microkernel-dev`, and
`scripts/env.sh` drives it with `podman run`.

### Decision

Keep the existing `Containerfile` and `scripts/env.sh`. The module gives only
`scripts/enter-dev.sh`, which delegates to `env.sh shell`. The dev-environment
skill documents this workflow.

### Alternatives

- Migrate to Podman Compose with a `dev` service (a second entry point for the
  same image)

### Consequences

- One entry point (`env.sh`) for configure, build, test, clean, and shell.
- The image build and the container flags stay in one script.

<!-- [CAT] Structure -->

## ADR-3 (2026-11-20): Move tests and test components into tests/

### Context

The test harness lived in `scripts/`, and the test applications `ipccli` and
`sigtest` lived in `apps/`. The test files had no single root folder.

### Decision

Collect all test files under `tests/` at the repository root:
`tests/test_runner.py`, `tests/ipccli/`, and `tests/sigtest/`. The `apps/`
directory is gone. Test applications are Ring 3 programs. The build packs them
into `boot.tar` as before.

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

The project had no license file. Contributors and distributors need clear
license terms.

### Decision

Add `LICENSE.md` with the MIT License. The copyright line is `Copyright (c) 2026
Deniz Eren <deniz.eren@outlook.com>`. Each source file gets this line as a
header comment, with an `SPDX-License-Identifier: MIT` line below it.

### Alternatives

- GPL-family licenses (strong copyleft, heavier compliance for distributors)
- Apache-2.0 (explicit patent grant, longer notice text)
- BSD-2/3-Clause (shorter, no trademark clause)

### Consequences

- Distributors must keep the copyright notice and the license text.
- Each new source file starts with the copyright header (see
  [CONVENTIONS.md](CONVENTIONS.md)).
- `docs/THIRD-PARTY-NOTICES.md` keeps the third-party attributions separate.

<!-- [CAT] Governance -->

## ADR-1 (2026-11-20): Adopt agent-scaffolding

### Context

The project must have governance, documentation, and agent instructions that
work with all AI agents.

### Decision

Apply the agent-scaffolding agent skill 0.2.0 with the configuration in
`.agent-scaffolding.yml`. The commit mode is AUTONOMOUS.

### Alternatives

- Documents and agent instructions that you write for each project
- Instructions for one platform only (for example only `CLAUDE.md` or only
  `.github/copilot-instructions.md`)

### Consequences

- `AGENTS.md` is the supreme authority for agent behavior.
- Procedures are only in `docs/skills/`.
- The state documents (`docs/TASKS.md`, `CHANGELOG.md`, and `docs/DECISIONS.md`)
  are up to date before each commit.
