# Documentation Contributor Guide

This guide explains the docs-as-code traceability model of `microkernel`. The
Sphinx build excludes this file.

## Layout and Model

This layout organizes documents by engineering abstraction level from
`00-overview/` through `60-verification/`, plus project records in `records/`.
Write each item one time as a Sphinx-Needs directive with a permanent ID, one
status, and forward links only. Sphinx-Needs computes all back-links, indexes,
and coverage views.

## Syntax, IDs, Links, Statuses, and Tags

- **Markup:** Primary markup is configured in `.docs-traceability-setup.yml`
  (MyST fences or RST directives). Nested child items use four backticks on the
  parent directive.
- **IDs:** Permanent IDs follow the `id.style` configured in
  `.docs-traceability-setup.yml` (`slug` or fixed-width zero-padded `number`).
  Do not reuse or rename an ID. Update its status instead.
- **Links:** Write forward links only (`derives`, `satisfies`, `refines`,
  `verifies`, `drives`, `governs`, `traces`, `supersedes`).
- **Statuses:** Normative items and conventions use `draft`, `active`, or
  `obsolete`. Decisions use `proposed`, `accepted`, `superseded`, or
  `deprecated`. Planning records use `open`, `in_progress`, `done`, or
  `cancelled`.
- **Tags:** Mark non-verified requirements with `informational`. Give each `ver`
  item exactly one method tag from `model.ver_method_tags`.

## In-Code Traceability Tags

- Place comment tags `need:: <ID> [<ID> ...]` near the related implementation or
  in the module docstring (not in the file header block).
- Implementation code references only `ARC_`, `ICD_`, or `COMP_` IDs.
- Test files reference `VER_` IDs, and each test file must carry at least one
  `VER_` ID.
- Do not put `SYS_`, `SWR_`, `ADR_`, `BDR_`, `WDR_`, `CNV_`, `TSK_`, or `RDM_`
  IDs in source code.

## Installing Dependencies

Make a local virtual environment and install the documentation dependencies:

```bash
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
```

Alternatively, CMake can also make an isolated build venv with
`-DDOCS_AUTO_INSTALL=ON`.

## Building and Testing

Configure, build HTML documentation (with warnings as errors), and run all
traceability checks:

```bash
cmake -B build
cmake --build build
ctest --test-dir build -L docs --output-on-failure
```

For standalone documentation builds without a root `CMakeLists.txt`, configure
with `cmake -S docs -B build`.

## Source Text Conventions

Source and documentation files follow an 80-column maximum line length, greedy
prose packing, and Doxygen file headers. The `docs-traceability-setup` Agent
Skill enforces these source-text rules directly. Skill settings live in
`.docs-traceability-setup.yml`, and editor formatting and `[file-header]` notice
lines live in `.editorconfig`.
