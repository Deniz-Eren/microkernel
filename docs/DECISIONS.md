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

## ADR-14 (2026-11-21): Add SYS_SIGPROCMASK ABI and libc.a safety gate

### Context

`SWR_SIG_0003` and `SWR_SIG_0004` require standard POSIX.1-2024 `<signal.h>`
APIs (`sigaction`, `sigprocmask`, `pthread_sigmask`, `kill`, `pthread_kill`,
`raise`, `signal`, and `sigset_t` helpers), `-EINVAL` rejection of `SIGKILL` and
`SIGSTOP` overrides, signal mask save and restore across `SYS_SIGRETURN`, and
verified async-signal-safety. The Milestone 4 `<sys/signal.h>` exposed non-POSIX
inline syscall wrappers, leaked `<sys/microkernel.h>`, had no `SYS_SIGPROCMASK`
syscall, and left `sig_frame_t.pad` unused.

### Decision

- Separate the low-level kernel ABI (`libc/include/sys/signal.h`, `struct
  __k_sigaction`, `SYS_SIGPROCMASK = 27`) from the public POSIX.1-2024 header
  (`libc/include/signal.h`) and its out-of-line wrappers in `libc/src/signal.c`.
- Repurpose the 8-byte `pad` slot in `sig_frame_t` as `saved_mask` (keeping the
  16-byte aligned 24-uint64 frame size intact) so `signal_deliver_check` saves
  and `sys_sigreturn` restores `blocked_signals`, and store per-signal `sa_mask`
  and `sa_flags` arrays in `pcb_t`.
- Add CTest `posix.async_signal_safe` (`tests/posix/test_async_signal_safe.py`)
  running `llvm-objdump -dr` on `sysroot/usr/lib/libc.a` to parse direct calls
  and `R_X86_64_*` relocations and fail if any async-signal-safe function
  transitively reaches heap, stdio, or `exit` routines.

### Alternatives

- Pass `struct sigaction` directly as the kernel ABI struct (couples kernel
  layout to future `siginfo_t` / `sa_sigaction` extensions)
- Audit async-signal-safety only by manual source review instead of an automated
  binary call-graph test

### Consequences

- `<signal.h>` compiles standalone under strict C17 with both
  `_POSIX_C_SOURCE=202405L` and `_XOPEN_SOURCE=800` without leaking `SYS_*`.
- `sigtest` verifies `SIGKILL`/`SIGSTOP` `EINVAL`, `sigprocmask` deferral, and
  `sig_frame_t` mask restoration in headless QEMU.
- Every future change to `libc.a` is automatically checked for
  async-signal-safety regressions by CTest.

## ADR-13 (2026-11-21): Align .clang-format to 80 cols and exempt immutable docs

### Context

Full-repository `docs-traceability-setup` `check_text.py` validation revealed a
conflict between `.clang-format` (`ColumnLimit: 100`) and `.editorconfig`
(`max_line_length = 80`), legacy 2-line copyright headers on pre-existing C/H,
CMake, and shell files, and false findings on verbatim `LICENSE.md` and
immutable vendor specifications under `docs/skills/*/references/`.

### Decision

Set `ColumnLimit: 80` in `.clang-format` to match `.editorconfig`, add
`max_line_length = off` exemptions in `.editorconfig` only for verbatim
`LICENSE.md` and immutable `docs/skills/*/references/*`, upgrade all existing
C/H, CMake, shell, and Python files to Doxygen `@file`/`@brief`/`@copyright`
headers, and zero-initialize syscall output structs in `servers/` and `tests/`
so 80-column formatting stays clean under `clang-tidy` without fragile trailing
`NOLINT` comments.

### Alternatives

- Raise `.editorconfig` `max_line_length` to 100 for C/H files (diverges from
  the uniform 80-column repository convention)
- Leave pre-existing files out of `check_text.py` checks permanently

### Consequences

- Running `check_text.py --root .` across the entire repository exits with code
  0 and zero findings.
- `clang-format` and `.editorconfig` agree on 80 columns across all source
  files, while immutable vendor specs remain untouched.

## ADR-12 (2026-11-21): Enforce ASD-STE100 and modal_verb=must for requirements

### Context

ADR-5 established ASD-STE100 Simplified Technical English (Issue 9) for project
documentation, while `.docs-traceability-setup.yml` initially kept the skill's
default `authoring.modal_verb: shall`. In ASD-STE100, `shall` is not an approved
modal verb (only `can`, `cannot`, `must`, and `will` are approved), and an audit
with `ste_check.py` over extracted MyST `{sys}`/`{swr}` bodies found 59 errors
(`MODAL: shall` and sentences over the 25-word Rule 6.3 limit).

### Decision

Set `authoring.modal_verb: must` in `.docs-traceability-setup.yml`, revise all
requirements documents in `docs/20-system/` to comply with ASD-STE100 Issue 9
(using `must`, active voice, and sentences of at most 25 words), update the
project glossaries (`technical_terms.txt`, `allowed_phrases.txt`), and make
ASD-STE100 compliance for all requirements explicit in `AGENTS.md`, `WDR_0002`,
and `CNV_0002`.

### Alternatives

- Keep `shall` in requirements and exclude `docs/20-system/` from ASD-STE100
- Rewrite prose without updating `technical_terms.txt` for domain OS terms

### Consequences

- Requirements and governance docs use one consistent controlled language
  (ASD-STE100 Issue 9) with zero `ste_check.py` errors across extracted
  `{sys}`/`{swr}` bodies.
- Future agents see the ASD-STE100 requirement directly in `AGENTS.md` and the
  pre-commit checklist.

## ADR-11 (2026-11-21): Enforce two-tier bidirectional POSIX/code traceability

### Context

End-to-end verification of POSIX.1-2024 requirements demands unbroken links from
standard clauses down to headers, C implementations, and automated tests. Until
now, `need_tag_check.py` only checked tags present in code without flagging
active `ICD_`, `COMP_`, or automated `VER_` items that lacked code tags, most
existing kernel and `libc` sources carried no `need::` tags, and POSIX
references existed only as prose links.

### Decision

Enforce the two-tier bidirectional traceability methodology codified in
`WDR_0002`, `CNV_0001`, `CNV_0004`, and `AGENTS.md`:

- Add an optional `:posix:` field to `config/sphinx/conf.py` and render a
  calculated POSIX.1-2024 Compliance Matrix in
  `docs/60-verification/traceability.md`.
- Extend `config/sphinx/need_tag_check.py` (`find_untagged_active_needs`) to
  fail the Sphinx `-W` build whenever any active `ICD_`, `COMP_`, or automated
  (non-`analysis`) `VER_` item has zero in-code `need::` tags.
- Split `LIBC` interfaces/components by header group (`ICD_LIBC_0001`–`0003`,
  `COMP_LIBC_0001`–`0002`) and add `ICD_`/`COMP_` tags across all existing
  `kernel/`, `libc/`, `servers/`, `config/sphinx/`, and `tools/traceability/`
  implementations.
- Keep Sphinx-Needs items at the contract/header/subsystem level and rely on the
  automated `issue8-manifest.tsv` symbol gate for per-symbol POSIX completeness.

### Alternatives

- Create per-function Sphinx-Needs directives for every POSIX C function
- Leave code-to-spec coverage advisory instead of build-enforced

### Consequences

- Every active `ICD_`, `COMP_`, and automated `VER_` item is guaranteed by CI to
  trace to tracked source or test code.
- Adding future POSIX functions follows one documented rule in `AGENTS.md` and
  `WDR_0002` without Markdown directive bloat.

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
