# Architecture — DOCS

Domain `DOCS`: Architecture items for document traceability.

```{arc} Sphinx-Needs traceability build and check subsystem
:id: ARC_DOCS_0001
:status: active
:satisfies: SYS_DOCS_0001
:traces: ADR_0001

The document subsystem connects Sphinx, MyST-Parser, Sphinx-Needs, and the code
marker check tool to CMake and CTest. It also does the standalone `needs.json`
check ({need}`SYS_DOCS_0001`).
```

## Traceability and Four-Check Compatibility Model

Each active item connects from business decisions to source files and tests
without Markdown text for each function (`WDR_0002`). Sphinx `-W` and CTest do
traceability checks and POSIX.1-2024 compatibility checks on each commit.

```mermaid
flowchart TD
    BDR["BDR_ (Business Decisions)"]
    DEC["ADR_ and WDR_ (Architecture and Workflow Decisions)"]
    SYS["SYS_ (System Requirements)"]
    SWR["SWR_ (Software Requirements and :posix: Issue 8)"]
    ARC["ARC_ (Domain Architecture)"]
    ICD["ICD_ (Public Headers with need::)"]
    COMP["COMP_ (C Source Files with need::)"]
    VER["VER_ (test_* Files with need::)"]

    GATES["CTest: posix.headers, symbol_diff, async_safe, cancel, docs.audit"]

    BDR --> SYS
    SYS --> SWR
    DEC -.->|"traces"| SWR
    SWR --> ARC
    ARC --> ICD
    ARC --> COMP
    ICD --> COMP
    SWR --> VER
    ICD -.->|"need:: marker check"| GATES
    COMP -.->|"need:: marker check"| GATES
    VER -.->|"QEMU and CTest"| GATES
```
