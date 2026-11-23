# AI Agent Governance

This file has the highest authority for AI agents in **microkernel**. Skill
files in [docs/skills/](docs/skills/) hold full procedures. This file links to
them and does not repeat their content.

---

## Commit Mode

`operations.commit_mode` in `.agent-scaffolding.yml` is the single source of
truth for commit mode and commit signatures. Read the mode from there. The [git
skill](docs/skills/git/SKILL.md) defines each mode and its workflow.

---

## Predict -> Act -> Verify

1. **Predict:** examine the task, plan the steps, confirm Ring 0 vs. Ring 3
   placement under `CNV_0007` (keeping filesystems, paths, drivers, and named
   objects in Ring 3), and find possible failures before you act.
2. **Act:** do the planned steps. Stage any newly created files (`git add -N`)
   before running verification so `tag_scan.py` includes them.
3. **Verify (mandatory hard gate):** run `./scripts/build-agent.sh` and confirm
   it prints `BUILD: PASS` with exit code `0`. This single command enforces
   every required gate across the entire repository (never substitute partial or
   hand-picked file lists):
   - `clang-format` (`./scripts/formatter-agent.sh`)
   - `docs-traceability-setup` full-repo text check (`check_text.py --root .`)
   - `asd-ste100` full-repo STE check (`python3 tools/check_ste.py --root .`)
   - 4-stage build + Sphinx `-W`
   - headless QEMU test suite
   - `clang-tidy` static analysis
   - full CTest suite (`posix.headers`, `posix.symbol_diff`,
     `posix.async_signal_safe`, `posix.cancellation`,
     `arch.microkernel_boundary`, `traceability.unit`, `docs.html`,
     `docs.needs`, `docs.audit`)

If verification fails, go back to Act, repair every failure, and re-run
`./scripts/build-agent.sh` until it prints `BUILD: PASS`. Never run `git commit`
before `./scripts/build-agent.sh` passes on the final working tree. Also keep
the local git `pre-commit` hook installed via `./scripts/install-hooks.sh`
(never pass `--no-verify`).

---

## Failure Modes

| Failure | Step | If it continues |
| --- | --- | --- |
| Build fails | Fix root cause and rebuild | After 3 tries, ask human |
| Test fails | Fix failure source and retest | After 3 tries, ask human |
| Config rejected | Drop 1 option, test, log ADR | Empty config, log risk |
| Tool missing | Note in tool skill, log ADR | None |
| Git conflict | Stop, pull, resolve, retry | Ask human |
| Near token limit | Summarize and link files | None |

---

## Boundaries for CLI Commands

| Command | Permitted | Approval |
| --- | --- | --- |
| Read files and `--version` checks | Yes | No |
| Run build, test, format, and lint scripts | Yes | No |
| Commit | See `commit_mode` in `.agent-scaffolding.yml` | No |
| Install project deps in dev container | Yes | No |
| Install system packages | No | Yes |
| Erase project files | No | Yes |
| Change IMMUTABLE documents | No | Not permitted |
| Push to a remote | No | Yes |

**Build Agent First:** delegate build, test, and format runs to the subagents in
[docs/agents/](docs/agents/). If the platform has no subagents, run the scripts.
Report only the summary line and failures.

---

## Token and Context Management

- Load a skill only when necessary for the task.
- Link to skill files and documents. Do not copy their content.
- Give a summary of earlier context. Do not repeat it.
- Read connected files in one batch.
- Write `~` or `$HOME`. Do not write an absolute home path.

---

## Git Protocol

Read identity from `.agent-scaffolding.yml` (`identity.git_author_name`,
`identity.git_author_email`). The value `current_user` uses the environment git
config (`git config user.name`, `git config user.email`). Pass the identity
inline for each commit and do not change git config:

```bash
git -c user.name="<name>" -c user.email="<email>" commit -F <message-file>
```

Each commit message obeys Conventional Commits with an empty line and an
`Assisted-by:` footer naming the agent and model of the **current session**:

```text
<type>[scope]: <description>

Assisted-by: <AgentName>:<Model> [+ <Tool> ...]
```

For the full procedure, see the [git skill](docs/skills/git/SKILL.md).

---

## Pre-Commit Checklist

Before every `git commit`, you MUST run `./scripts/build-agent.sh` on the final
working tree (with new files `git add -N`'d) and confirm `BUILD: PASS`. Never
skip gates, never check only a hand-picked subset of files, and never pass
`--no-verify` to bypass `.git/hooks/pre-commit`.

- [ ] `./scripts/build-agent.sh` ran on the final working tree and exited 0
  (`FORMAT: PASS`, full-repo `check_text.py --root .`, full-repo
  `tools/check_ste.py --root .`, 4-stage build + Sphinx `-W`, QEMU tests,
  `clang-tidy`, and all CTest targets)
- [ ] Commit message obeys Conventional Commits with `Assisted-by:` footer
- [ ] All new or changed interfaces, components, and test files carry valid
  `need::` tags (`ICD_`, `COMP_`, `VER_`)
- [ ] `README.md` user-space support breakdown is updated whenever user-space
  headers, functions, syscalls, or Ring 3 services change
- [ ] `docs/records/TASKS.md` and `docs/records/HISTORY.md` updated (move
  completed `TSK_` items verbatim to `HISTORY.md` with `:status: done`)
- [ ] `CHANGELOG.md` has an entry in `## [Unreleased]`
- [ ] `docs/30-architecture/adr/`, `docs/10-product/bdr/`, or
  `docs/00-overview/wdr/` has a decision record (`ADR_`, `BDR_`, or `WDR_`) for
  each new decision
- [ ] `docs/THIRD-PARTY-NOTICES.md` attributes each new third-party component
- [ ] All new and changed source files have the copyright header
- [ ] Microkernel boundary preserved (`CNV_0007`): no Ring 3 policy, path
  storage, device driver, or named-object registry added to `kernel/`; any new
  `SYS_*` syscall or `kernel/core/*.c` module is justified in an accepted `ADR_`
  and registered in `tests/arch/microkernel_allowlist.json`
  (`arch.microkernel_boundary` passes)
- [ ] New or changed `POSIX` wording uses goal-oriented `POSIX.1-2024` phrasing
  with no compliance claims, no `POSIX` product names, and original text only
- [ ] No IMMUTABLE document was changed

For update procedures, see
[state-management](docs/skills/state-management/SKILL.md).

---

## Rules for This Project

Rules that apply only to this project. Each rule links to its skill or document.

- **Minimal Ring 0 microkernel architecture (`CNV_0007`, equal priority with
  POSIX.1-2024):** Upholding the microkernel boundary is mandatory and must
  never be traded away for implementation convenience.
  - **Allowed in Ring 0 (`kernel/`):** only mechanisms that require CPU
    privilege or direct hardware and address-space control: 256-level SMP
    scheduling and IPIs, synchronous and scatter-gather IPC, threads and
    `FS.base` TLS, futexes, signals, clocks and timers, PMM/VMM (`W^X`, COW,
    `SHM`), `CAP_SYS_INTR` IRQ/MMIO/DMA/PCI routing, minimal FD slot dispatch,
    and `syscall`/`sysret`.
  - **Forbidden in Ring 0 (must live in Ring 3 `libc/` or `servers/`):**
    filesystems, pathname parsing or path storage, named object registries (such
    as named semaphores or service tables), device drivers, pipe/socket/network
    protocols, and high-level POSIX abstractions composable over `SYS_SHM_OP`,
    `MAP_SHARED`, `SYS_FUTEX`, and IPC channels.
  - **Hard gate on Ring 0 expansion:** before adding any `SYS_*` syscall, new
    `kernel/core/*.c` file, or new field on `struct __k_fd_entry`, prove the
    feature cannot be built in Ring 3, record an accepted `ADR_`, and update
    `tests/arch/microkernel_allowlist.json` (enforced by CTest
    `arch.microkernel_boundary`). Keep C code concise and consistent with
    existing modules (minimal comments). See
    [ADR_0002](docs/30-architecture/adr/ADR_0002.md),
    [ADR_0031](docs/30-architecture/adr/ADR_0031.md),
    [WDR_0010](docs/00-overview/wdr/WDR_0010.md), and
    [docs/records/CONVENTIONS.md](docs/records/CONVENTIONS.md) (`CNV_0007`).
- **Source-text and `.editorconfig` compliance (`docs-traceability-setup`):**
  All new and changed files must obey `.editorconfig` (UTF-8, LF, trailing
  newline, max line width) and pass the `docs-traceability-setup` skill's
  `scripts/check_text.py` (maximum line length, greedy prose packing, and
  Doxygen file headers) at all times alongside `clang-format`, `clang-tidy`,
  `asd-ste100`, `posix-issue8-os-dev`, and CTest. Repair content rather than
  weakening rules. See `.editorconfig`, `.docs-traceability-setup.yml`, and
  [docs/records/CONVENTIONS.md](docs/records/CONVENTIONS.md) (`CNV_0002`,
  `CNV_0003`).
- **ASD-STE100 technical English (`asd-ste100`):** All documentation and
  requirements in `README.md` and `docs/` (including Sphinx-Needs `{sys}`,
  `{swr}`, and specification directives) must follow ASD-STE100 Issue 9. Always
  use the `asd-ste100` skill to write and check requirements, use `must` (not
  `shall`) as the modal verb (`authoring.modal_verb: must`), and keep project
  terms in `technical_terms.txt` and `allowed_phrases.txt`. See
  [docs/records/CONVENTIONS.md](docs/records/CONVENTIONS.md) (`CNV_0002`).
- **Two-tier bidirectional code and POSIX traceability
  (`posix-issue8-os-dev`):** Keep Sphinx-Needs items at the contract, header, or
  subsystem level with `:posix:` references; tag headers with `ICD_`,
  implementations with `COMP_`, and tests (`test_*`) with `VER_`; never create
  per-function Markdown boilerplate. See
  [WDR_0002](docs/00-overview/wdr/WDR_0002.md),
  [ADR_0003](docs/30-architecture/adr/ADR_0003.md), and
  [docs/records/CONVENTIONS.md](docs/records/CONVENTIONS.md) (`CNV_0001`,
  `CNV_0004`).
- **POSIX wording, originality, and trademark rules:** Frame standard support as
  ongoing work toward `POSIX.1-2024` (`IEEE Std 1003.1-2024` / `ISO/IEC/IEEE
  9945:2026`) using goal-oriented phrasing (for example, "working toward
  POSIX.1-2024 compatibility" or "targets interfaces from IEEE Std
  1003.1-2024"). Pin references to the 2024 edition unless documenting legacy
  rejection or compatibility. Never claim completion or certification (avoid
  "POSIX certified", "POSIX conformant", "POSIX compliant", or "implements
  POSIX"), never put `POSIX` in OS, project, binary, or component names, and
  never copy verbatim text from copyrighted IEEE or Open Group specifications.
  Keep the notice `POSIX® is a registered trademark of the Institute of
  Electrical and Electronic Engineers, Inc.` in public-facing docs. See
  [BDR_0003](docs/10-product/bdr/BDR_0003.md) and
  [docs/THIRD-PARTY-NOTICES.md](docs/THIRD-PARTY-NOTICES.md).
- **Keep `README.md` user-space support current:** Whenever you add, change, or
  remove public headers, `libc` functions, syscalls, or Ring 3 system services,
  you must update the user-space functionality breakdown and use-case summary in
  [README.md](README.md) in the same commit so application developers always see
  accurate current capabilities.
- **Mermaid diagrams for concepts and architecture:** When documenting a
  subsystem, architecture, protocol, or workflow, add a focused Mermaid diagram
  where it helps illustrate the system or concept—keep diagrams purposeful and
  avoid unnecessary clutter. Prefer vertical orientation (`flowchart TD`,
  `direction TB`) over wide horizontal layouts so diagrams stay readable, and
  avoid volatile test or item counts inside diagrams.
- **Milestone discipline:** Make one milestone at a time and commit after each
  complete milestone. A milestone is complete only when its test gives exit code
  0. See [docs/records/TASKS.md](docs/records/TASKS.md) and
  [docs/records/CONVENTIONS.md](docs/records/CONVENTIONS.md) (`CNV_0006`).
- **Freestanding C17:** The kernel and C library are freestanding C17 with
  warnings as errors. The kernel runs with no C runtime. See
  [docs/skills/cmake/SKILL.md](docs/skills/cmake/SKILL.md).
