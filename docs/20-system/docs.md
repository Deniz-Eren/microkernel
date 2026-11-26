# System and Software Requirements — DOCS

The `DOCS` domain includes the traceability build, Sphinx-Needs model checks,
two-direction `need::` marker checks, and POSIX.1-2024 compatibility gates.

```{sys} Do automatic traceability checks in the project build
:id: SYS_DOCS_0001
:status: active

The build system must examine traceability relations, coverage rules, and code
markers automatically. It must stop the build if a check shows a warning or an
error.
```

```{swr} Examine exported Sphinx-Needs models for full traceability
:id: SWR_DOCS_0001
:status: active
:derives: SYS_DOCS_0001

The model auditor must examine two-direction relations, permitted relation
types, requirement coverage, verification methods, and domain and decision file
names in exported `needs.json` ({need}`SYS_DOCS_0001`).
```

```{swr} Examine two-direction code traceability markers in tracked files
:id: SWR_DOCS_0002
:status: active
:derives: SYS_DOCS_0001
:traces: WDR_0002

The marker scanner and Sphinx extension must examine tracked source and test
files for `need::` markers and prefix rules. Each test file must have a `VER_`
marker, and each active `ICD_`, `COMP_`, and automatic `VER_` item must have a
code marker ({need}`SYS_DOCS_0001`).
```

```{swr} Do POSIX.1-2024 header, symbol, safety, and runtime checks
:id: SWR_DOCS_0003
:status: active
:posix: XBD/2 (BASE), XBD/13-14 (BASE), XSH/2 (BASE)
:derives: SYS_DOCS_0001
:traces: BDR_0003, ADR_0003, WDR_0002

The test harness must do 4 POSIX.1-2024 compatibility gates: isolated header
compile checks, symbol checks for `issue8-manifest.tsv`, async-signal-safety and
cancellation call graph checks, and runtime compatibility test suites
({need}`SYS_DOCS_0001`).
```

```{swr} Examine multi-subsystem integration, pselect, and memory leaks
:id: SWR_DOCS_0004
:status: active
:posix: XBD/sys/select.h (BASE), XSH/pselect (BASE)
:derives: SYS_DOCS_0001
:traces: BDR_0003, ADR_0003, ADR_0023

The system must give `<sys/select.h>` (`select`, `pselect`) through multi-server
`ppoll` with Gate 1 to Gate 3 checks. An integrated QEMU suite must make sure
that no PMM frame, descriptor, or server slot leaks occur across `sh`, VFS,
pipes, TTY, sockets, and `SHM`. It must compare accurate kernel frame counts and
Ring 3 server slot counts before and after test cycles ({need}`SYS_DOCS_0001`).
```

```{swr} Make automatic Ring 0 and Ring 3 LLVM code coverage when selected
:id: SWR_DOCS_0005
:status: active
:derives: SYS_DOCS_0001
:traces: ADR_0025

With `-DMICROKERNEL_COVERAGE=ON`, the build and QEMU test harness must collect
LLVM `Raw Profile v8` coverage data across Ring 0 and Ring 3. The kernel must
write the coverage data as base64 frames through `arch_console_write()` with no
filesystem or `x86-64` device dependency. `./scripts/env.sh coverage` must start
`llvm-profdata` and `llvm-cov` in `build-cov/` to make HTML, LCOV, and text
reports, but default builds must stay without coverage code
({need}`SYS_DOCS_0001`).
```

```{swr} Examine microkernel Ring 0 boundary allowlist in CTest
:id: SWR_DOCS_0006
:status: active
:derives: SYS_DOCS_0001
:traces: ADR_0002, WDR_0010

The test harness must examine each `kernel/core/*.c` module, each `SYS_*`
syscall in `libc/include/sys/microkernel.h`, and each `struct __k_fd_entry`
field against `tests/arch/microkernel_allowlist.json`. Each named `ADR_` must
have the status `accepted` ({need}`SYS_DOCS_0001`).
```
