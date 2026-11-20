# System and Software Requirements — DOCS

Domain `DOCS`: Docs-as-code traceability build, Sphinx-Needs model auditing, and
in-code traceability tag validation.

```{sys} Enforce automated end-to-end engineering traceability
:id: SYS_DOCS_0001
:status: active

The build system shall validate all traceability links, coverage rules, and
in-code tags automatically and fail the build on any warning or audit finding.
```

```{swr} Audit exported Sphinx-Needs models for completeness
:id: SWR_DOCS_0001
:status: active
:derives: SYS_DOCS_0001

The standalone model auditor shall verify link symmetry, permitted link types,
requirement coverage, verification method tags, and domain/decision file rules
against exported `needs.json` ({need}`SYS_DOCS_0001`).
```

```{swr} Validate in-code traceability tags across tracked files
:id: SWR_DOCS_0002
:status: active
:derives: SYS_DOCS_0001

The tag scanner and Sphinx extension shall extract comment-anchored `need::`
tags across tracked source and test files, enforce prefix placement rules, and
require at least one `VER_` tag per test file ({need}`SYS_DOCS_0001`).
```
