# Conventions

## Purpose

Record the active engineering conventions (`con`) of `microkernel`.

## Structure

Each convention appears under a `### CNV_<SUFFIX> — Title` heading followed by
one `{con}` directive tracing the decision that established it.

## Formatting Rules

- Update each convention in place so it always reflects current practice.
- Mark retired conventions with `:status: obsolete`.
- Never reuse or rename an assigned ID.
- Record every deviation from a convention as a decision (`ADR_`, `BDR_`, or
  `WDR_`).

```{needtable} All conventions
:types: con
:columns: id; title; status; traces as "Traces to"; traces_back as "Traced by"
```

### CNV_0001 — In-Code Traceability Tags

```{con} In-code traceability tags
:id: CNV_0001
:status: active
:traces: ADR_0001, WDR_0002

Put standalone `need:: <ID>` comments near code or in module docstrings, outside
the Doxygen header. Tag headers with `ICD_`, source modules with `COMP_`, and
tests with `VER_`. Never put `SYS_` or `SWR_` IDs in code. All active `ICD_`,
`COMP_`, and automated `VER_` items must have a `need::` tag. Name new POSIX C
tests `test_*.c`.
```

### CNV_0002 — Source Text and ASD-STE100 Conventions

```{con} Source text and ASD-STE100 conventions
:id: CNV_0002
:status: active
:traces: ADR_0001, WDR_0002, WDR_0006, WDR_0007, WDR_0008

Wrap all source and doc files to the `.editorconfig` 80-column limit and pack
prose greedily. Pass `scripts/check_text.py --root .` with zero findings. Write
all requirements (`sys`, `swr`) and docs in ASD-STE100 Issue 9 using active
voice and `must` (not `shall`). Keep sentences within 25 words and pass
`tools/check_ste.py`. Keep project terms in `technical_terms.txt` and
`allowed_phrases.txt`.
```

### CNV_0003 — File Header Blocks and Copyright Notices

```{con} File header blocks and copyright notices
:id: CNV_0003
:status: active
:traces: ADR_0001, WDR_0004, WDR_0007

Use the MIT License notice `Copyright (c) 2026 Deniz Eren
<deniz.eren@outlook.com>` and `SPDX-License-Identifier: MIT`. Start each C/C++
file (`.c`, `.h`) with a `/** @file ... @brief ... @copyright` Doxygen block.
Start each Python, CMake, or shell file with a `##` Doxygen block after any
shebang. Use `/* ... */` copyright lines on `.S` assembly files and `# ...`
lines on `Containerfile` and `grub.cfg`. Exempt Markdown, `.gitignore`,
`LICENSE.md`, and upstream LLVM headers in `sysroot/` (attributed in
`docs/THIRD-PARTY-NOTICES.md`).
```

### CNV_0004 — POSIX.1-2024 Header Hygiene and Two-Tier Traceability

```{con} POSIX.1-2024 header hygiene and two-tier traceability
:id: CNV_0004
:status: active
:traces: ADR_0003, WDR_0002

Write POSIX.1-2024 Sphinx-Needs items at the header, contract, or subsystem
level with `:posix:` refs and `9799919799` URLs. Use the `issue8-manifest.tsv`
gate instead of per-function Markdown. Scope `ICD_` and `COMP_` by header or
feature group. Reserve internal names under `__` or `_[A-Z]`, and never clear
`errno` on success.
```

### CNV_0005 — File Naming and Documentation Rules

```{con} File Naming and Documentation Rules
:id: CNV_0005
:status: active
:traces: WDR_0003, WDR_0005, WDR_0009

Use UPPERCASE names for record files in `docs/records/` (`TASKS.md`,
`ROADMAP.md`, `HISTORY.md`, `CONVENTIONS.md`) and `docs/THIRD-PARTY-NOTICES.md`.
Place agent skills in `docs/skills/<lowercase-name>/SKILL.md` matching
frontmatter `name`, and agent definitions in `docs/agents/<lowercase-kebab>.md`.
Use lowercase C names with no spaces, and put each Ring 3 test program in its
own directory under `tests/`. Keep tool procedures only in `docs/skills/`, never
wrap Markdown links in backticks, and record deviations as decisions.
```

### CNV_0006 — Milestone Discipline and Include Paths

```{con} Milestone Discipline and Include Paths
:id: CNV_0006
:status: active
:traces: WDR_0003, WDR_0008, WDR_0009

Implement one milestone or task at a time and commit only after
`./scripts/build-agent.sh` prints `BUILD: PASS` with exit code 0. Do not start
the next milestone before the current automated test passes. Record defect
repairs in the commit message. Use CMake target include directories (`#include
<arch/...>`) instead of parent-relative `../` `#include` paths in C sources.
```
