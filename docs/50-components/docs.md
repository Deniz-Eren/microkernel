# Components — DOCS

Domain `DOCS`: White-box specifications for the traceability model auditor and
bidirectional in-code tag scanner.

```{comp} Standalone Sphinx-Needs model auditor
:id: COMP_DOCS_0001
:status: active
:refines: ARC_DOCS_0001, ICD_DOCS_0001
:satisfies: SWR_DOCS_0001

`tools/traceability/check_needs_audit.py` loads `needs.json` and checks link
symmetry, link rules, coverage, tags, and filenames. `tools/check_ste.py` runs
ASD-STE100 checks across `README.md` and `docs/` ({need}`ICD_DOCS_0001`,
{need}`ARC_DOCS_0001`).
```

```{comp} Bidirectional in-code tag scanner and Sphinx validation extension
:id: COMP_DOCS_0002
:status: active
:refines: ARC_DOCS_0001, ICD_DOCS_0001
:satisfies: SWR_DOCS_0002

`config/sphinx/tag_scan.py` and `config/sphinx/need_tag_check.py` scan tracked
files for `need::` tags and render `need-code-trace`. They stop the build on bad
tags, untagged tests, or active `ICD_`/`COMP_`/`VER_` items with no code tags
({need}`ICD_DOCS_0001`, {need}`ARC_DOCS_0001`).
```
