# AI Agent Governance

This file has the highest authority for AI agents in **microkernel**. Skill
files in [docs/skills/](docs/skills/) keep full procedures. This file connects
to them and does not write their content again.

---

## Fast Commands and Mandatory Rules

Obey these rules on each task before you examine files or start commands:

- **Relative paths only:** Always use relative paths from the repository root
  (for example `libc/ldso/ldso.c`, `.agents/skills/git/SKILL.md`). Do not write
  `/home/...` paths in tool calls or commands.
- **Read the codebase map first:** Read
  [docs/00-overview/codebase-map.md](docs/00-overview/codebase-map.md) for build
  paths, `boot.tar` and VFS rules, FD startup, syscall headers, and virtual
  memory constants.
- **Keep context small:** Use `read` with `offset` and `limit` as an alternative
  to `bash` `cat` or `sed`. To examine one task without all of
  `docs/records/TASKS.md`, start `./scripts/env.sh task TSK_NNNN`.
- **Container commands:** Start toolchain and LLVM commands with
  `./scripts/env.sh <cmd...>` (for example `./scripts/env.sh llvm-readelf -d
  build/ld.so`). Do not give commands to `./scripts/env.sh shell`. Do not give
  host `/tmp/...` paths to container commands. Do not use `nohup ... &` with
  `sleep`. Do not write custom Python ELF parsers.
- **Fast iteration and pre-commit checks:** Use `./scripts/env.sh build-code`
  for fast C and ISO builds that skip Sphinx docs. Use
  `./scripts/formatter-agent.sh --fix` for C format (do not wrap C lines by
  hand). Start `./scripts/env.sh next-ids` to find the next traceability IDs.
  Start `./scripts/build-agent.sh` before each commit.
- **Debug procedure (no loops):** On a QEMU error, timeout, or incorrect output:
  (1) read [docs/skills/qemu-debug/SKILL.md](docs/skills/qemu-debug/SKILL.md),
  (2) use `llvm-addr2line` as an alternative to manual `objdump` offsets, (3)
  for freestanding `ld.so` debug lines use `SYS_DEBUG_LOG` (writes to serial
  when `fd 1` is a pipe), and (4) after two failed hypotheses, stop and read
  your new helper functions line by line.
- **STE and traceability defaults:** Keep descriptive sentences under 25 words,
  procedural sentences under 20 words, and paragraphs under 6 sentences. Use
  `must` (not `shall`), active voice, no semicolons, and no contractions. Add
  one `SWR_` and one `VER_` for each task unless the task changes two or more
  subsystems.

---

## Commit Mode

`operations.commit_mode` in `.agent-scaffolding.yml` is the one source of truth
for commit mode and commit signatures. Read the mode from that file. The [git
skill](docs/skills/git/SKILL.md) gives each mode and its workflow.

---

## Plan -> Do -> Check

1. **Plan:** Examine the task and plan the steps. Make sure that Ring 0 and Ring
   3 placement obeys `CNV_0007` (keep filesystems, paths, drivers, and named
   objects in Ring 3). Find possible errors before you start.
2. **Do:** Do the planned steps. Add new files to the git index (`git add -N`)
   before you start checks so that `tag_scan.py` includes them.
3. **Check (mandatory check):** Start `./scripts/build-agent.sh` and make sure
   that it writes `BUILD: PASS` with exit code `0`. This one command does all
   mandatory checks across the full repository (do not examine a subset of
   files):
   - `clang-format` (`./scripts/formatter-agent.sh`)
   - `docs-traceability-setup` full-repo text check (`check_text.py --root .`)
   - optional STE check (`python3 tools/check_ste.py --root .`)
   - four-step build with Sphinx `-W`
   - headless QEMU test suite
   - `clang-tidy` static analysis
   - full CTest suite (`posix.headers`, `posix.symbol_diff`,
     `posix.async_signal_safe`, `posix.cancellation`,
     `arch.microkernel_boundary`, `traceability.unit`, `docs.html`,
     `docs.needs`, `docs.audit`)

If a check stops with an error, go back to Do, repair each error, and start
`./scripts/build-agent.sh` again until it writes `BUILD: PASS`. Do not start
`git commit` before `./scripts/build-agent.sh` succeeds on the last source tree.
Also keep the local git `pre-commit` script installed through
`./scripts/install-hooks.sh`. Do not use `--no-verify`.

---

## Error Modes

| Error | Step | If it continues |
| --- | --- | --- |
| Build stops | Repair root cause and build again | After 3 tries, tell human |
| Test stops | Repair error and do test again | After 3 tries, tell human |
| Config rejected | Remove 1 option, do test, add ADR | Empty config, add risk |
| Tool missing | Record in tool skill, record ADR | None |
| Git clash | Stop, pull, repair, try again | Tell human |
| Near token limit | Give summary and link files | None |

---

## Limits for CLI Commands

| Command | Permitted | Approval |
| --- | --- | --- |
| Read files and `--version` checks | Yes | No |
| Start build, test, format, and lint scripts | Yes | No |
| Make a commit | Refer to `commit_mode` in `.agent-scaffolding.yml` | No |
| Install project dependencies in dev container | Yes | No |
| Install system packages | No | Yes |
| Erase project files | No | Yes |
| Change IMMUTABLE documents | No | Not permitted |
| Push to a remote | No | Yes |

**Build Agent First:** Give build, test, and format tasks to the subagents in
[docs/agents/](docs/agents/). If the platform has no subagents, start the
scripts directly. Write only the summary line and errors.

---

## Token and Context Management

- Load a skill only when necessary for the task.
- Connect to skill files and documents. Do not make a copy of their content.
- Give a summary of previous context. Do not write it again.
- Read connected files in one batch.
- Write `~` or `$HOME`. Do not write an absolute home path.

---

## Git Protocol

Read identity from `.agent-scaffolding.yml` (`identity.git_author_name`,
`identity.git_author_email`). The value `current_user` uses the environment git
config (`git config user.name`, `git config user.email`). Give the identity
inline for each commit and do not change git config:

```bash
git -c user.name="<name>" -c user.email="<email>" commit -F <message-file>
```

Each commit message obeys Conventional Commits with an empty line and an
`Assisted-by:` footer that names the agent and model of the **active session**:

```text
<type>[scope]: <description>

Assisted-by: <AgentName>:<Model> [+ <Tool> ...]
```

For the full procedure, refer to the [git skill](docs/skills/git/SKILL.md).

---

## Pre-Commit Checklist

Before each `git commit`, you MUST start `./scripts/build-agent.sh` on the last
source tree (with new files added through `git add -N`) and make sure that it
writes `BUILD: PASS`. Do not skip checks, do not examine a subset of files, and
do not use `--no-verify` to bypass `.git/hooks/pre-commit`.

- [ ] `./scripts/build-agent.sh` operated on the last source tree and gave exit
  code 0 (`FORMAT: PASS`, full-repo `check_text.py --root .`, optional
  `tools/check_ste.py --root .`, four-step build with Sphinx `-W`, QEMU tests,
  `clang-tidy`, and all CTest targets)
- [ ] Commit message obeys Conventional Commits with `Assisted-by:` footer
- [ ] All new or changed interfaces, components, and test files have correct
  `need::` markers (`ICD_`, `COMP_`, `VER_`)
- [ ] `README.md` user-space function summary changed when user-space headers,
  functions, syscalls, or Ring 3 services changed
- [ ] `docs/records/TASKS.md` and `docs/records/HISTORY.md` changed (completed
  `TSK_` items moved verbatim to `HISTORY.md` with `:status: done`)
- [ ] `CHANGELOG.md` has an entry in `## [Unreleased]`
- [ ] `docs/30-architecture/adr/`, `docs/10-product/bdr/`, or
  `docs/00-overview/wdr/` has a decision record (`ADR_`, `BDR_`, or `WDR_`) for
  each new decision
- [ ] `docs/THIRD-PARTY-NOTICES.md` records each new third-party component
- [ ] All new and changed source files have the copyright header
- [ ] Microkernel boundary stayed strict (`CNV_0007`): no Ring 3 rule, path
  storage, device driver, or named-object registry added to `kernel/`. Each new
  `SYS_*` syscall or `kernel/core/*.c` module has an accepted `ADR_` and an
  entry in `tests/arch/microkernel_allowlist.json` (`arch.microkernel_boundary`
  succeeds)
- [ ] New or changed `POSIX` text uses goal-oriented `POSIX.1-2024` text with no
  compliance claims, no `POSIX` component names, and new text only
- [ ] No IMMUTABLE document changed

For change procedures, refer to
[state-management](docs/skills/state-management/SKILL.md).

---

## Rules for This Project

Rules that apply only to this project. Each rule connects to its skill or
document.

- **Small Ring 0 microkernel architecture (`CNV_0007`, equal priority with
  POSIX.1-2024):** The microkernel boundary is mandatory. Do not break it for
  convenience.
  - **Permitted in Ring 0 (`kernel/`):** Only mechanisms where CPU privilege or
    hardware and address-space control is necessary. These are 256-level SMP
    scheduling and IPIs, synchronous and scatter-gather IPC, threads and
    `FS.base` TLS, futexes, signals, clocks, and timers. They also include
    PMM/VMM (`W^X`, COW, `SHM`), `CAP_SYS_INTR` IRQ, MMIO, DMA, and PCI control,
    small FD slot dispatch, and `syscall`/`sysret`.
  - **Not permitted in Ring 0 (must stay in Ring 3 `libc/` or `servers/`):**
    Filesystems, pathname parse code, path storage, and named object registries
    (for example named semaphores or service tables). Device drivers,
    pipe/socket and network protocols, and high-level POSIX interfaces built on
    `SYS_SHM_OP`, `MAP_SHARED`, `SYS_FUTEX`, and IPC channels also stay in Ring
    3.
  - **Hard check on Ring 0 growth:** Before you add a `SYS_*` syscall, new
    `kernel/core/*.c` file, or new field on `struct __k_fd_entry`, prove that
    Ring 3 cannot do the function. Record an accepted `ADR_` and change
    `tests/arch/microkernel_allowlist.json` (`arch.microkernel_boundary`
    examines this file). Keep C code short so that it agrees with active modules
    (small number of comments). Refer to
    [ADR_0002](docs/30-architecture/adr/ADR_0002.md),
    [ADR_0031](docs/30-architecture/adr/ADR_0031.md),
    [WDR_0010](docs/00-overview/wdr/WDR_0010.md), and
    [docs/records/CONVENTIONS.md](docs/records/CONVENTIONS.md) (`CNV_0007`).
- **Source-text and `.editorconfig` rules (`docs-traceability-setup`):** All new
  and changed files must obey `.editorconfig` (UTF-8, LF, last newline, max line
  width). They must also succeed on `scripts/check_text.py` (maximum line
  length, greedy prose layout, and Doxygen file headers) with `clang-format`,
  `clang-tidy`, `ste-writing-skill` (when installed), `posix-issue8-os-dev`, and
  CTest. Repair content as an alternative to weak rules. Refer to
  `.editorconfig`, `.docs-traceability-setup.yml`, and
  [docs/records/CONVENTIONS.md](docs/records/CONVENTIONS.md) (`CNV_0002`,
  `CNV_0003`).
- **Technical text (`ste-writing-skill`):** Use `ste-writing-skill` (STE) when
  you write and examine documents and requirements in `README.md` and `docs/`
  (with Sphinx-Needs `{sys}`, `{swr}`, and specification directives). Use `must`
  (not `shall`) as the modal verb (`authoring.modal_verb: must`). Keep project
  terms in `technical_terms.txt` and `allowed_phrases.txt`. Do not make a copy
  of content from `ste-writing-skill` into this repository. When
  `ste-writing-skill` is installed on the build host, `tools/check_ste.py`
  operates automatically. Build scripts skip it when missing. When
  `ste-writing-skill` is available, do a deep manual STE check of each changed
  document. Do not trust the script result alone. Examine sentence length,
  active voice, approved words, and simple tenses against the skill rules. When
  `ste-writing-skill` is not available, skip the manual check without a failure.
  Keep the English of all documents aligned to the style and simplicity of the
  existing text. Refer to
  [docs/records/CONVENTIONS.md](docs/records/CONVENTIONS.md) (`CNV_0002`).
- **Two-tier bidirectional code and POSIX traceability
  (`posix-issue8-os-dev`):** Keep Sphinx-Needs items at the interface, header,
  or subsystem level with `:posix:` links. Tag headers with `ICD_`, source
  modules with `COMP_`, and tests (`test_*`) with `VER_`. Do not make
  one-for-each-function Markdown boilerplate. Refer to
  [WDR_0002](docs/00-overview/wdr/WDR_0002.md),
  [ADR_0003](docs/30-architecture/adr/ADR_0003.md), and
  [docs/records/CONVENTIONS.md](docs/records/CONVENTIONS.md) (`CNV_0001`,
  `CNV_0004`).
- **POSIX text, originality, and trademark rules:** Frame standard work as an
  active goal for `POSIX.1-2024` (`IEEE Std 1003.1-2024` / `ISO/IEC/IEEE
  9945:2026`). Use goal-oriented text (for example, "targets compatibility with
  interfaces from POSIX.1-2024" or "targets interfaces from IEEE Std
  1003.1-2024"). Pin links to the 2024 edition unless you record older edition
  removal or compatibility. Do not claim full compliance or certification (do
  not write "POSIX certified", "POSIX conformant", "POSIX compliant", or
  "implements POSIX"). Do not put `POSIX` in OS, project, executable, or
  component names. Do not make copies of verbatim text from copyrighted IEEE or
  Open Group specifications. Keep the notice `POSIX® is a registered trademark
  of the Institute of Electrical and Electronic Engineers, Inc.` in public
  documents. Refer to [BDR_0003](docs/10-product/bdr/BDR_0003.md) and
  [docs/THIRD-PARTY-NOTICES.md](docs/THIRD-PARTY-NOTICES.md).
- **Keep `README.md` user-space status accurate:** When you change public
  headers, `libc` functions, syscalls, or Ring 3 services, you must change the
  user-space function summary in [README.md](README.md) in the same commit.
  Software authors will then always see accurate functions.
- **Mermaid diagrams for concepts and architecture:** When you document a
  subsystem, architecture, protocol, or workflow, add a focused Mermaid diagram
  where it helps show the system or concept. Keep diagrams purposeful and
  prevent clutter. Prefer vertical layout (`flowchart TD`, `direction TB`) to
  wide horizontal layouts so that diagrams stay easy to read. Do not put
  temporary test or item counts in diagrams.
- **Milestone discipline:** Complete one milestone at a time and make a commit
  after each completed milestone. A milestone is completed only when its test
  gives exit code 0. Refer to [docs/records/TASKS.md](docs/records/TASKS.md) and
  [docs/records/CONVENTIONS.md](docs/records/CONVENTIONS.md) (`CNV_0006`).
- **Freestanding C17:** The kernel and C library use freestanding C17 with
  warnings as errors. The kernel operates with no C runtime. Refer to
  [docs/skills/cmake/SKILL.md](docs/skills/cmake/SKILL.md).
