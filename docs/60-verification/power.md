# Verification — POWER

Domain `POWER`: Verification items covering power supply monitoring
requirements.

```{ver} Verify primary power supply supervision
:id: VER_POWER_0001
:status: active
:tags: integration
:verifies: SYS_POWER_0001

Verified by integration analysis of the primary power supply monitoring chain
({need}`SYS_POWER_0001`).
```

```{ver} Verify supply voltage bounds monitoring
:id: VER_POWER_0002
:status: active
:tags: integration
:verifies: SWR_POWER_0001

Verified by end-to-end threshold monitoring checks covering the supply monitor
requirement ({need}`SWR_POWER_0001`).
```

```{ver} Verify low-voltage fault detection
:id: VER_POWER_0003
:status: active
:tags: unit
:verifies: SWR_POWER_0002

Verified by `CheckNeedsAuditTests` in
`tests/traceability/test_check_needs_audit.py`, confirming low-voltage fault
traceability ({need}`SWR_POWER_0002`).
```

```{ver} Verify high-voltage fault detection
:id: VER_POWER_0004
:status: active
:tags: unit
:verifies: SWR_POWER_0003

Verified by `TagScanTests` in `tests/traceability/test_tag_scan.py`, confirming
high-voltage fault traceability ({need}`SWR_POWER_0003`).
```

```{ver} Verify system expectations end to end
:id: VER_POWER_0005
:status: active
:tags: integration
:verifies: SYS_POWER_0001

Verified by the headless QEMU serial-output harness `tests/test_runner.py`. It
boots the built ISO, checks the ordered milestone expectations on the serial
line, and gives exit code 0 only when all expectations match
({need}`SYS_POWER_0001`).
```
