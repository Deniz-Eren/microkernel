# Verification — DOCS

Domain `DOCS`: Verification items for the traceability model check and source
marker scanner.

```{ver} Test of traceability model check in CTest
:id: VER_DOCS_0001
:status: active
:tags: integration
:verifies: SYS_DOCS_0001, SWR_DOCS_0001

CTest `docs.audit` starts `tools/traceability/check_needs_audit.py` on the
exported `needs.json` model ({need}`SYS_DOCS_0001`, {need}`SWR_DOCS_0001`).
```

```{ver} Test of model check rules with unit tests
:id: VER_DOCS_0002
:status: active
:tags: unit
:verifies: SWR_DOCS_0001

`CheckNeedsAuditTests` in `tests/traceability/test_check_needs_audit.py` does
unit tests of the model check rules ({need}`SWR_DOCS_0001`).
```

```{ver} Test of bidirectional marker scanner and coverage rules
:id: VER_DOCS_0003
:status: active
:tags: unit
:verifies: SWR_DOCS_0002

`TagScanTests` in `tests/traceability/test_tag_scan.py` makes sure that the
scanner parses comment-anchored markers across the repository. It also makes
sure that the scanner finds active `ICD_`, `COMP_`, and `VER_` items that have
no code markers ({need}`SWR_DOCS_0002`).
```

```{ver} Test of four POSIX.1-2024 compatibility checks in CTest
:id: VER_DOCS_0004
:status: active
:tags: integration
:verifies: SWR_DOCS_0003

CTest `posix.headers`, `posix.symbol_diff`, `posix.async_signal_safe`, and
`posix.cancellation` do checks across `sysroot` and `libc.a`. The QEMU test
harness also does runtime tests ({need}`SWR_DOCS_0003`).
```

```{ver} Test of select, pselect, and resource-load cycles in QEMU
:id: VER_DOCS_0005
:status: active
:tags: integration
:verifies: SWR_DOCS_0004

`tests/stresstest/test_stress.c` does a check of `<sys/select.h>` (`select`,
`pselect`, multi-server FDs, and atomic `sigmask` `EINTR`). It also makes sure
that PMM frame, FD, SHM, pipe, VFS, and socket counts stay equal across load
cycles. CTest compatibility gates 1 to 3 compile `<sys/select.h>` and do a check
of its symbols and cancellation points ({need}`SWR_DOCS_0004`).
```

```{ver} Test of framed LLVM coverage decode and report workflow
:id: VER_DOCS_0006
:status: active
:tags: unit
:verifies: SWR_DOCS_0005

CTest `coverage.unit` (`tests/coverage/test_generate_report.py`) makes sure that
the coverage tool parses base64 frames, reads magic bytes, finds ELF
`__llvm_covmap` sections, and starts `llvm-cov`. During `./scripts/env.sh
coverage`, `tests/test_runner.py --cov-out --wait-cov` collects Ring 0 and Ring
3 coverage data ({need}`SWR_DOCS_0005`).
```

```{ver} Test of microkernel Ring 0 boundary allowlist in CTest
:id: VER_DOCS_0007
:status: active
:tags: unit
:verifies: SWR_DOCS_0006

CTest `arch.microkernel_boundary` (`tests/arch/test_microkernel_boundary.py`)
makes sure that all `kernel/core/*.c` modules, `SYS_*` syscalls, and `struct
__k_fd_entry` fields agree with `tests/arch/microkernel_allowlist.json` and
connect to accepted ADRs ({need}`SWR_DOCS_0006`).
```
