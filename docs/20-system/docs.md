# System and Software Requirements — DOCS

Domain `DOCS`: Docs-as-code traceability build, Sphinx-Needs model auditing,
bidirectional in-code tag validation, and POSIX.1-2024 conformance gates.

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

```{swr} Enforce bidirectional in-code traceability tags across tracked files
:id: SWR_DOCS_0002
:status: active
:derives: SYS_DOCS_0001
:traces: WDR_0002

The tag scanner and Sphinx extension shall extract comment-anchored `need::`
tags across tracked source and test files, enforce prefix placement rules,
require at least one `VER_` tag per test file, and fail the build if any active
`ICD_`, `COMP_`, or automated (non-`analysis`) `VER_` item has no in-code tag
({need}`SYS_DOCS_0001`).
```

```{swr} Automate POSIX.1-2024 header, symbol, safety, and runtime gates
:id: SWR_DOCS_0003
:status: draft
:posix: XBD/2 (BASE), XBD/13-14 (BASE), XSH/2 (BASE)
:derives: SYS_DOCS_0001
:traces: BDR_0003, ADR_0003, WDR_0002

The verification harness shall execute four POSIX.1-2024 conformance gates:
standalone header compilation under `-D_POSIX_C_SOURCE=202405L`, manifest
symbol/namespace diffing against `issue8-manifest.tsv`, async-signal-safety and
cancellation-point call-graph checks, and runtime POSIX test suites
({need}`SYS_DOCS_0001`).
```
