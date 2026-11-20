# Components — DOCS

Domain `DOCS`: White-box specifications for the traceability model auditor and
bidirectional in-code tag scanner.

```{comp} Standalone Sphinx-Needs model auditor
:id: COMP_DOCS_0001
:status: active
:refines: ARC_DOCS_0001, ICD_DOCS_0001
:satisfies: SWR_DOCS_0001

`tools/traceability/check_needs_audit.py` loads exported `needs.json` and checks
link symmetry, link matrix compliance, requirement coverage, verification method
tags, and domain/decision filenames ({need}`ICD_DOCS_0001`) for the
documentation subsystem ({need}`ARC_DOCS_0001`).
```

```{comp} Bidirectional in-code tag scanner and Sphinx validation extension
:id: COMP_DOCS_0002
:status: active
:refines: ARC_DOCS_0001, ICD_DOCS_0001
:satisfies: SWR_DOCS_0002

`config/sphinx/tag_scan.py` and `config/sphinx/need_tag_check.py` scan tracked
source and test files for comment-anchored `need::` tags, render the
`need-code-trace` table, and emit build-breaking warnings on invalid tags,
untagged test files, or active `ICD_`, `COMP_`, and automated `VER_` items
missing in-code tags ({need}`ICD_DOCS_0001`) for the documentation subsystem
({need}`ARC_DOCS_0001`).
```
