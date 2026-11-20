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

```{needtable} All conventions
:types: con
:columns: id; title; status; traces as "Traces to"; traces_back as "Traced by"
```

### CNV_0001 — In-Code Traceability Tags

```{con} In-code traceability tags
:id: CNV_0001
:status: active
:traces: ADR_0001, WDR_0002

Use standalone comment lines starting with `need:: <ID>` near related code or
inside the module docstring, outside the Doxygen file header block. Tag public
headers with `ICD_`, implementation modules with `COMP_`, and test files with
`VER_`. Never place `SYS_` or `SWR_` IDs in source code. Every active `ICD_`,
`COMP_`, and automated (non-`analysis`) `VER_` item must have at least one
in-code `need::` tag, and new POSIX C test files must follow `test_*.c` naming.
```

### CNV_0002 — Source Text and ASD-STE100 Conventions

```{con} Source text and ASD-STE100 conventions
:id: CNV_0002
:status: active
:traces: ADR_0001, WDR_0002

Wrap all new and changed source and documentation files to the `.editorconfig`
column limit, pack prose paragraphs greedily, and verify zero findings with the
`docs-traceability-setup` skill's `scripts/check_text.py` at all times. Write
all requirements (`sys`, `swr`) and docs in ASD-STE100 Simplified Technical
English (Issue 9) with the `asd-ste100` skill: use `must` (not `shall`) as the
modal verb (`authoring.modal_verb: must`), keep requirement sentences to 25
words or fewer, use active voice, and maintain project terms in
`technical_terms.txt` and `allowed_phrases.txt`.
```

### CNV_0003 — File Header Blocks

```{con} File header blocks
:id: CNV_0003
:status: active
:traces: ADR_0001

Start every human-authored Python, CMake, shell, and C/C++ file with a Doxygen
header block carrying `@file`, `@brief`, and `@copyright` matching the
`[file-header]` section of `.editorconfig`.
```

### CNV_0004 — POSIX.1-2024 Header Hygiene and Two-Tier Traceability

```{con} POSIX.1-2024 header hygiene and two-tier traceability
:id: CNV_0004
:status: active
:traces: ADR_0003, WDR_0002

Author POSIX.1-2024 Sphinx-Needs items at the header, behavioral-contract, or
subsystem level with a `:posix:` reference and canonical `9799919799` URLs; rely
on the automated `issue8-manifest.tsv` symbol gate rather than per-function
Markdown directives. Keep `ICD_` and `COMP_` scoped per header or feature group,
verify Base versus option-group tags against the manifest, reserve internal
names under `__` or `_[A-Z]`, and never set `errno` to `0` or clear it on
success.
```
