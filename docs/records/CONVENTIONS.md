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
:traces: ADR_0001

Use comment lines starting with `need:: <ID>` near related code or inside the
module docstring. Implementation code links only `ARC_`, `ICD_`, or `COMP_` IDs;
test files link `VER_` IDs.
```

### CNV_0002 — Source Text Conventions

```{con} Source text conventions
:id: CNV_0002
:status: active
:traces: ADR_0001

Wrap source and documentation files to the column limit configured in
`.editorconfig` and pack prose paragraphs greedily as validated by the
`docs-traceability-setup` skill.
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

### CNV_0004 — POSIX.1-2024 Header Hygiene and Spec Citations

```{con} POSIX.1-2024 header hygiene and spec citations
:id: CNV_0004
:status: active
:traces: ADR_0003

Cite canonical Issue 8 (`9799919799`) specification URLs in POSIX requirements
and reviews. Verify mandatory Base versus option-group tags against the Issue 8
manifest, keep internal identifiers in `__` or `_[A-Z]`, and never set `errno`
to `0` or clear it on success.
```
