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

`TagScanTests` in `tests/traceability/test_tag_scan.py` checks comment-anchored
tag extraction and repository scanning. It also checks bidirectional detection
of active `ICD_`, `COMP_`, and `VER_` items with no code tags
({need}`SWR_DOCS_0002`).
```

```{ver} Verify 4-gate POSIX.1-2024 compatibility harness in CTest
:id: VER_DOCS_0004
:status: active
:tags: integration
:verifies: SWR_DOCS_0003

Verified by CTest `posix.headers`, `posix.symbol_diff`,
`posix.async_signal_safe`, and `posix.cancellation` across `sysroot` and
`libc.a`, paired with headless QEMU tests ({need}`SWR_DOCS_0003`).
```

```{ver} Verify select, pselect, and cross-subsystem zero-leak stress in QEMU
:id: VER_DOCS_0005
:status: active
:tags: integration
:verifies: SWR_DOCS_0004

`tests/stresstest/test_stress.c` verifies `<sys/select.h>` (`select`, `pselect`,
multi-server FDs, atomic `sigmask` `EINTR`) and exact PMM frame, FD, SHM, pipe,
VFS, and socket handle equality across stress rounds. CTest Gates 1 to 3 verify
`<sys/select.h>` compilation, symbols, and cancellation ({need}`SWR_DOCS_0004`).
```

```{ver} Verify framed LLVM profile decoding and coverage report pipeline
:id: VER_DOCS_0006
:status: active
:tags: unit
:verifies: SWR_DOCS_0005

CTest `coverage.unit` (`tests/coverage/test_generate_report.py`) verifies base64
frame decoding, magic validation, ELF `__llvm_covmap` detection, and `llvm-cov`
commands. `tests/test_runner.py --cov-out --wait-cov` captures Ring 0 and Ring 3
profiles during `./scripts/env.sh coverage` ({need}`SWR_DOCS_0005`).
```
