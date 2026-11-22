# System and Software Requirements — DOCS

Domain `DOCS`: docs-as-code traceability build, Sphinx-Needs model audit,
bidirectional tag checks, and POSIX.1-2024 compatibility gates.

```{sys} Run automated traceability checks in the project build
:id: SYS_DOCS_0001
:status: active

The build system must validate traceability links, coverage rules, and in-code
tags automatically. It must stop the build if a check reports a warning or audit
error.
```

```{swr} Audit exported Sphinx-Needs models for completeness
:id: SWR_DOCS_0001
:status: active
:derives: SYS_DOCS_0001

The standalone model auditor must validate link symmetry, permitted link types,
requirement coverage, method tags, and domain/decision file names in exported
`needs.json` ({need}`SYS_DOCS_0001`).
```

```{swr} Examine bidirectional in-code traceability tags in tracked files
:id: SWR_DOCS_0002
:status: active
:derives: SYS_DOCS_0001
:traces: WDR_0002

The tag scanner and Sphinx extension must scan tracked source and test files for
`need::` tags and examine prefix rules. Each test file must have a `VER_` tag,
and each active `ICD_`, `COMP_`, and automated `VER_` item must have an in-code
tag ({need}`SYS_DOCS_0001`).
```

```{swr} Run POSIX.1-2024 header, symbol, safety, and runtime gates
:id: SWR_DOCS_0003
:status: active
:posix: XBD/2 (BASE), XBD/13-14 (BASE), XSH/2 (BASE)
:derives: SYS_DOCS_0001
:traces: BDR_0003, ADR_0003, WDR_0002

The test harness must run four POSIX.1-2024 compatibility gates: standalone
header compilation, `issue8-manifest.tsv` symbol diff, async-signal-safety and
cancellation call-graph checks, and runtime compatibility test suites
({need}`SYS_DOCS_0001`).
```

```{swr} Verify cross-subsystem integration, pselect, and leak freedom
:id: SWR_DOCS_0004
:status: draft
:posix: XBD/sys/select.h (BASE), XSH/pselect (BASE)
:derives: SYS_DOCS_0001
:traces: BDR_0003, ADR_0003

The system must supply `<sys/select.h>` (`select`, `pselect`) over multi-server
`ppoll` with Gate 1 to Gate 3 checks. An integrated QEMU stress suite must
verify zero PMM frame, descriptor, or server handle leaks across `sh`, VFS,
pipes, TTY, sockets, and `SHM`. It must check exact kernel frame counts and Ring
3 server handle counts before and after stress loops ({need}`SYS_DOCS_0001`).
```
