# Architecture — DOCS

Domain `DOCS`: Structural architecture elements for docs-as-code traceability.

```{arc} Sphinx-Needs traceability build and checker subsystem
:id: ARC_DOCS_0001
:status: active
:satisfies: SYS_DOCS_0001
:traces: ADR_0001

The documentation subsystem integrates Sphinx, MyST-Parser, Sphinx-Needs, and
the code-tag checker into CMake and CTest. It also runs the standalone
`needs.json` auditor ({need}`SYS_DOCS_0001`).
```

## Bidirectional Traceability and Four-Gate Compatibility Model

Every normative item links down from product decisions to source files and tests
without per-function boilerplate (`WDR_0002`). Sphinx `-W` and CTest run both
traceability checks and POSIX.1-2024 compatibility checks on every commit.

```mermaid
flowchart TD
    BDR["BDR_ (Business Decisions)"]
    DEC["ADR_ / WDR_ (Architecture & Workflow Decisions)"]
    SYS["SYS_ (System Requirements)"]
    SWR["SWR_ (Software Requirements + :posix: Issue 8 Refs)"]
    ARC["ARC_ (Domain Architecture)"]
    ICD["ICD_ (Public Headers via need::)"]
    COMP["COMP_ (C Sources via need::)"]
    VER["VER_ (test_* Files via need::)"]

    GATES["CTest: posix.headers, symbol_diff, async_safe, cancel, docs.audit"]

    BDR --> SYS
    SYS --> SWR
    DEC -.->|"traces"| SWR
    SWR --> ARC
    ARC --> ICD
    ARC --> COMP
    ICD --> COMP
    SWR --> VER
    ICD -.->|"need:: tag scan"| GATES
    COMP -.->|"need:: tag scan"| GATES
    VER -.->|"QEMU + CTest"| GATES
```
