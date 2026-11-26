# Conventions

## Purpose

Record the active project conventions (`con`) of `microkernel`.

## Structure

Each convention shows below a `### CNV_<SUFFIX> — Title` title followed by one
`{con}` directive that connects to the decision that made it.

## Format Rules

- Change each convention in position so that it always shows active practice.
- Set `:status: obsolete` on retired conventions.
- Do not use an assigned ID again or change its name.
- Record each change from a convention as a decision (`ADR_`, `BDR_`, or
  `WDR_`).

```{needtable} All conventions
:types: con
:columns: id; title; status; traces as "Traces to"; traces_back as "Traced by"
```

### CNV_0001 — In-Code Traceability Markers

```{con} In-code traceability markers
:id: CNV_0001
:status: active
:traces: ADR_0001, WDR_0002

Put standalone `need:: <ID>` comments near code or in module docstrings,
external to the Doxygen header. Tag headers with `ICD_`, source modules with
`COMP_`, and tests with `VER_`. Do not put `SYS_` or `SWR_` IDs in code. Do not
refer to temporary `TSK_` or `RDM_` IDs in source code or comments. All active
`ICD_`, `COMP_`, and automatic `VER_` items must have a `need::` marker. Name
new POSIX C tests `test_*.c`.
```

### CNV_0002 — Source Text and STE Conventions

```{con} Source text and STE conventions
:id: CNV_0002
:status: active
:traces: ADR_0001, WDR_0002, WDR_0006, WDR_0007, WDR_0008

Keep all source files and documents in the 80-column limit in `.editorconfig`
with greedy prose layout. Make sure that `scripts/check_text.py --root .`
succeeds with no errors. Use `ste-writing-skill` (STE) for requirements (`sys`,
`swr`) and documents with modal verb `must` (not `shall`). Do not make a copy of
content from `ste-writing-skill` into this repository. When `ste-writing-skill`
is installed, make sure that `tools/check_ste.py` succeeds. Keep project terms
in `technical_terms.txt` and `allowed_phrases.txt`.
```

### CNV_0003 — File Header Blocks and Copyright Notices

```{con} File header blocks and copyright notices
:id: CNV_0003
:status: active
:traces: ADR_0001, WDR_0004, WDR_0007

Use the MIT License notice `Copyright (c) 2026 Deniz Eren
<deniz.eren@outlook.com>` and `SPDX-License-Identifier: MIT`. Start each C/C++
file (`.c`, `.h`) with a `/** @file ... @brief ... @copyright` Doxygen block.
Start each Python, CMake, or shell file with a `##` Doxygen block after a
shebang. Use `/* ... */` copyright lines on `.S` assembly files and `# ...`
lines on `Containerfile` and `grub.cfg`. Do not include Markdown, `.gitignore`,
`LICENSE.md`, or upstream LLVM headers in `sysroot/` (recorded in
`docs/THIRD-PARTY-NOTICES.md`).
```

### CNV_0004 — POSIX.1-2024 Header Hygiene and Two-Tier Traceability

```{con} POSIX.1-2024 header hygiene and two-tier traceability
:id: CNV_0004
:status: active
:traces: ADR_0003, BDR_0004, WDR_0002

Write POSIX.1-2024 Sphinx-Needs items at the header, interface, or subsystem
level with `:posix:` links and `9799919799` URLs. Use the `issue8-manifest.tsv`
check as an alternative to one-for-each-function Markdown. Scope `ICD_` and
`COMP_` by header or option group. Reserve internal names with `__` or `_[A-Z]`,
and do not set `errno` to 0 on success. Keep user-space interfaces in
POSIX.1-2024, and record each non-POSIX interface in a `BDR_` or `ADR_` before
work starts ({need}`BDR_0004`).
```

### CNV_0005 — File Names and Document Rules

```{con} File names and document rules
:id: CNV_0005
:status: active
:traces: WDR_0003, WDR_0005, WDR_0009

Use UPPERCASE names for record files in `docs/records/` (`TASKS.md`,
`ROADMAP.md`, `HISTORY.md`, `CONVENTIONS.md`) and `docs/THIRD-PARTY-NOTICES.md`.
Put agent skills in `docs/skills/<lowercase-name>/SKILL.md` with the same
frontmatter `name`. Put agent files in `docs/agents/<lowercase-kebab>.md`. Use
lowercase C names with no spaces, and put each Ring 3 test executable in its own
folder in `tests/`. Keep tool procedures only in `docs/skills/`. Do not put
Markdown links in backticks. Record each deviation as a decision.
```

### CNV_0006 — Milestone Discipline and Include Paths

```{con} Milestone discipline and include paths
:id: CNV_0006
:status: active
:traces: WDR_0003, WDR_0008, WDR_0009

Complete one milestone or task at a time and make a commit only after
`./scripts/build-agent.sh` writes `BUILD: PASS` with exit code 0. Do not start
the next milestone before the active automatic test succeeds. Record defect
repairs in the commit message. Use CMake target include directories (`#include
<arch/...>`) as an alternative to parent-relative `../` `#include` paths in C
sources.
```

### CNV_0007 — Microkernel Ring 0 Smallness and Code Style

```{con} Microkernel Ring 0 smallness and code style
:id: CNV_0007
:status: active
:traces: ADR_0002, ADR_0010, ADR_0017, ADR_0027, ADR_0031, WDR_0010

Keep Ring 0 (`kernel/`) restricted to privileged mechanisms. Permitted Ring 0
mechanisms are scheduling, SMP and IPIs, IPC channels, threads and TLS, futexes,
signals, and clocks or timers. They also include PMM/VMM (`W^X`, COW, `SHM`),
`CAP_SYS_INTR` hardware IRQs and port I/O, small FD slot dispatch, and
`syscall`/`sysret`. Keep filesystems, pathname parse code, path storage, device
drivers, named object registries, and protocol servers in Ring 3 (`libc/` or
`servers/`). Do not add a `SYS_*` syscall, `kernel/core/*.c` module, or `struct
__k_fd_entry` field without an accepted `ADR_`. Also add an entry in
`tests/arch/microkernel_allowlist.json` (`arch.microkernel_boundary`). Keep C
code short with a small number of comments that agree with active modules.
```
