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
:traces: ADR_0001, WDR_0002

Wrap all source and doc files to the `.editorconfig` limit and pack prose
greedily. Pass `scripts/check_text.py --root .` with zero findings. Write all
requirements (`sys`, `swr`) and docs in ASD-STE100 Issue 9. Use `must` (not
`shall`), keep sentences within 25 words, and pass `tools/check_ste.py`. Keep
project terms in `technical_terms.txt` and `allowed_phrases.txt`.
```

### CNV_0003 — File Header Blocks

```{con} File header blocks
:id: CNV_0003
:status: active
:traces: ADR_0001

Start each Python, CMake, shell, and C/C++ file with a Doxygen header block.
Include `@file`, `@brief`, and `@copyright` from `.editorconfig`.
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
