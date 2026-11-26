# Document Author Instructions

This document tells authors how the docs-as-code traceability model of
`microkernel` operates. The Sphinx build does not include this file.

## Layout and Model

This layout puts documents in folders by abstraction level from `00-overview/`
through `60-verification/`, with project records in `records/`. Write each item
one time as a Sphinx-Needs directive with a permanent ID, one status, and
forward links only. Sphinx-Needs calculates all back-links, indexes, and
coverage views.

## Syntax, IDs, Links, Statuses, and Markers

- **Markup:** `.docs-traceability-setup.yml` sets the primary markup (MyST
  fences or RST directives). Nested child items use four backticks on the parent
  directive.
- **IDs:** Permanent IDs obey the `id.style` option in
  `.docs-traceability-setup.yml` (`slug` or fixed-width `number`). Do not use an
  ID again or change its name. Change its status as an alternative.
- **Links:** Write forward links only (`derives`, `satisfies`, `refines`,
  `verifies`, `drives`, `governs`, `traces`, `supersedes`).
- **Statuses:** Normative items and conventions use `draft`, `active`, or
  `obsolete`. Decisions use `proposed`, `accepted`, `superseded`, or
  `deprecated`. Plan records use `open`, `in_progress`, `done`, or `cancelled`.
- **Markers:** Tag requirements without tests as `informational`. Give each
  `ver` item one method marker from `model.ver_method_tags`.

## In-Code Traceability Markers

- Put comment markers `need:: <ID> [<ID> ...]` near the related code or in the
  module comment block (not in the file header block).
- Code files refer only to `ARC_`, `ICD_`, or `COMP_` IDs.
- Test files refer to `VER_` IDs. Each test file must have a minimum of one
  `VER_` ID.
- Do not put `SYS_`, `SWR_`, `ADR_`, `BDR_`, `WDR_`, `CNV_`, `TSK_`, or `RDM_`
  IDs in source code.

## Install Dependencies

Make a local virtual environment and install the document dependencies:

```bash
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
```

As an alternative, CMake can make an isolated build virtual environment with
`-DDOCS_AUTO_INSTALL=ON`.

## Build and Do Checks

Configure CMake, make the HTML documents (with warnings as errors), and do all
traceability checks:

```bash
cmake -B build
cmake --build build
ctest --test-dir build -L docs --output-on-failure
```

To build documents alone without a root `CMakeLists.txt`, configure with `cmake
-S docs -B build`.

## Source Text Conventions

Source files and documents obey an 80-column maximum line length, greedy prose
layout, and Doxygen file headers. The `docs-traceability-setup` skill makes sure
that files obey these source-text rules. `.docs-traceability-setup.yml` keeps
skill options. `.editorconfig` keeps editor format rules and `[file-header]`
notice lines.
