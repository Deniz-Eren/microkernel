# Verification — DOCS

Domain `DOCS`: Verification items covering traceability model auditing and
in-code tag scanning.

```{ver} Verify end-to-end traceability model audit in CTest
:id: VER_DOCS_0001
:status: active
:tags: integration
:verifies: SYS_DOCS_0001, SWR_DOCS_0001

Verified by CTest `docs.audit` executing
`tools/traceability/check_needs_audit.py` against the exported `needs.json`
model ({need}`SYS_DOCS_0001`, {need}`SWR_DOCS_0001`).
```

```{ver} Verify model audit rules with unit tests
:id: VER_DOCS_0002
:status: active
:tags: unit
:verifies: SWR_DOCS_0001

Verified by `CheckNeedsAuditTests` in
`tests/traceability/test_check_needs_audit.py` ({need}`SWR_DOCS_0001`).
```

```{ver} Verify bidirectional tag scanner and coverage rules with unit tests
:id: VER_DOCS_0003
:status: active
:tags: unit
:verifies: SWR_DOCS_0002

Verified by `TagScanTests` in `tests/traceability/test_tag_scan.py`, covering
comment-anchored tag extraction, repository scanning, and bidirectional
detection of active `ICD_`, `COMP_`, and automated `VER_` items lacking code
tags ({need}`SWR_DOCS_0002`).
```
