## Purpose

Record the important changes of microkernel. The format follows [Keep a
Changelog](https://keepachangelog.com).

## Structure

- The `## [Unreleased]` section is at the top. Below it, the releases are newest
  first (git log order).
- Category headings of level 3 in each section: `### Added`, `### Changed`, `###
  Deprecated`, `### Removed`, `### Fixed`, and `### Security`. Put a heading
  into the file only when the section has content for it.

## Formatting Rules

- Release heading: `## [x.y.z] - YYYY-MM-DD`.
- The entries in each category are newest first (git log order).
- Write each entry as a complete sentence with a final period.

## [Unreleased]

### Added

- POSIX.1-2024 Base `<pthread.h>` and `<semaphore.h>` plus hardware x86-64
  `FS.base` TLS (`TSK_0006`, `SWR_PROC_0005`): syscalls `SYS_THREAD_SET_FSBASE`
  (`39`), `SYS_THREAD_EXIT` (`40`), and `SYS_FUTEX` (`41`), O(1) TLS `errno`,
  mutexes, `pthread_cond_clockwait`, barriers, spinlocks, rwlocks, TSD keys,
  async-signal-safe `sem_post`, QEMU `tests/pthreadtest`, and CTest coverage
  (ADR-19).
- POSIX.1-2024 Base monotonic/realtime clocks and interval timers (`TSK_0005`,
  `SWR_PROC_0006`): syscalls `30`–`38`, `kernel/core/time.c`, `<time.h>`,
  async-signal-safe `libc/src/time.c`, `sleep()` backed by `clock_nanosleep`,
  QEMU `tests/timetest`, and `posix.headers`/`posix.async_signal_safe` coverage
  (ADR-18).
- Unified pre-commit quality gate (`tools/check_ste.py`, updated
  `scripts/build-agent.sh`, and `scripts/install-hooks.sh`) enforcing full-repo
  `clang-format`, `docs-traceability-setup` `check_text.py --root .`,
  `asd-ste100` `ste_check.py`, build, QEMU tests, `clang-tidy`, and CTest before
  every commit (ADR-17).
- Milestone 6 (`TSK_0004`) hardware interrupt, port I/O, and scatter-gather IPC
  syscalls: `SYS_INTR_ATTACH_EVENT` (`11`), `SYS_INTR_UNMASK` (`12`),
  `SYS_IOPL_SET` (`25`, `CAP_SYS_INTR`-guarded), `SYS_MSG_READV` (`8`), and
  `SYS_MSG_WRITEV` (`9`), backed by IOAPIC routing (`kernel/core/intr.c`) and
  pre-allocated IRQ IPC pulses.
- Public `<sys/intr.h>` header (`AttachIntrEvent`, `IntrUnmask`, `IOPLSet`,
  `port_in*`/`port_out*`), `ReadMsgV`/`WriteMsgV`/`ipc_iov_t` in `<sys/ipc.h>`,
  Ring 3 MC146818 CMOS RTC driver (`servers/rtcdrv`, `/dev/rtc`), and
  unprivileged client test (`tests/drvtest`).
- Milestone 5 (`TSK_0002`) process and memory syscalls: `SYS_PROC_CREATE`,
  `SYS_PROC_WAIT` (`28`), `SYS_VM_MAP`, `SYS_VM_UNMAP`, `SYS_VM_CLONE_COW`, and
  `SYS_VM_PROTECT` (`29`), backed by per-frame PMM reference counts, Ring 3
  `#PF` COW fault handling, and user PML4 reclamation on child reap.
- Public POSIX.1-2024 `<sys/mman.h>` (`mmap`, `munmap`, `mprotect`, `msync`) and
  `<sys/wait.h>` (async-signal-safe `wait`, `waitpid`, `W*` macros) plus native
  `<sys/proc.h>` and `<sys/vm.h>` headers and `libc` wrappers.
- Automated Milestone 5 verification (`tests/proctest/main.c` and `child.c`)
  running concurrently with Milestones 1–4 in QEMU, plus `<sys/mman.h>` /
  `<sys/wait.h>` coverage in `posix.headers` and `wait`/`waitpid` in
  `posix.async_signal_safe`.
- Standard POSIX.1-2024 `<signal.h>` (`libc/include/signal.h`) and
  async-signal-safe `libc/src/signal.c` implementing `sigaction`, `sigprocmask`,
  `pthread_sigmask`, `kill`, `pthread_kill`, `raise`, `signal`, and `sigset_t`
  helpers, plus kernel `SYS_SIGPROCMASK` (`27`) and `sig_frame_t.saved_mask`
  save/restore across `SYS_SIGRETURN`.
- Gate 3 CTest `posix.async_signal_safe`
  (`tests/posix/test_async_signal_safe.py`) auditing `sysroot/usr/lib/libc.a`
  with `llvm-objdump -dr`, plus `<signal.h>` standalone coverage in
  `posix.headers` and QEMU runtime checks in `tests/sigtest/main.c`.
- Two-tier bidirectional code and POSIX.1-2024 traceability: `:posix:` field and
  calculated compliance matrix in `docs/60-verification/traceability.md`,
  bidirectional `ICD_`/`COMP_`/`VER_` coverage check in
  `config/sphinx/need_tag_check.py`, `WDR_0002`, `ICD_`/`COMP_` code tags across
  all active `kernel/`, `libc/`, `servers/`, and traceability tooling files, and
  governance rules in `AGENTS.md`.
- `<errno.h>` and lock-free TID-indexed thread-local `errno`
  (`libc/src/errno.c`) with the full POSIX.1-2024 `E*` constant set in
  `libc/include/sys/errno.h`.
- Standalone POSIX.1-2024 header compilation and namespace hygiene CTest gate
  (`posix.headers` via `tests/posix/test_headers.py`) and multithreaded `errno`
  verification in `tests/sigtest/main.c`.
- POSIX.1-2024 (Issue 8) Base and option-group software requirements across
  `LIBC`, `SIG`, `MEM`, `PROC`, `IPC`, and `DOCS` with canonical `9799919799`
  specification URLs, plus `ADR_0003`, `CNV_0004`, and `TSK_0003`.
- Real traceability domains (`BOOT`, `MEM`, `PROC`, `IPC`, `SIG`, `LIBC`, and
  `DOCS`) across all Sphinx-Needs layers, plus `BDR_0002`, `BDR_0003`
  (POSIX.1-2024 Base conformance target), `ADR_0002`, and `microkernel` tasks
  and roadmap records.
- The docs-as-code traceability model with the docs-traceability-setup agent
  skill 0.1.0. The configuration is in `.docs-traceability-setup.yml`.
- The Sphinx documentation in `docs/` with MyST-Parser and Sphinx-Needs, built
  by CMake with warnings as errors and checked by CTest (unit tests, HTML build,
  needs export, model audit).
- The traceability example items for the `POWER` domain, the decision records,
  and the project records in `docs/records/`.
- The EditorConfig file `.editorconfig` with the file header notice lines, and
  the Python dependency manifest `requirements.txt`.

### Changed

- Updated `scripts/install-hooks.sh` so `.git/hooks/pre-commit` also runs full
  CTest (`traceability.unit`, `docs.html`, `docs.needs`, `docs.audit`,
  `posix.headers`, `posix.async_signal_safe`) with fresh `build/docs/doctrees`.
- Expanded `docs/00-overview/glossary.md` to explain Ring 0, Ring 3, syscalls,
  PCB/TCB, IPC, PMM/VMM, PML4, SMEP/SMAP, COW, bootfs, sysroot, and
  async-signal-safety.
- Updated `docs/TASKS.md` and `docs/records/TASKS.md` with near-term open tasks
  (`TSK_0004`–`TSK_0007`) covering Milestone 6 and the next POSIX.1-2024 Base
  building blocks, and archived completed `RDM_0001` into
  `docs/records/HISTORY.md`.
- Updated `README.md` with a user-space functionality support table, current
  application use-cases, and an overview of the bidirectional Sphinx-Needs
  traceability setup, and added a rule in `AGENTS.md` requiring agents to keep
  the `README.md` support breakdown up to date.
- Updated `README.md` to highlight the POSIX.1-2024 (Issue 8) Base compliance
  target, CTest commands, and links to `BDR_0003`, `ADR_0003`, and the
  traceability matrix.
- Aligned `.clang-format` (`ColumnLimit: 80`) with `.editorconfig`, added
  `.editorconfig` exemptions for verbatim `LICENSE.md` and immutable
  `docs/skills/*/references/*`, upgraded all existing C/H, CMake, shell, and
  Python files to Doxygen headers, and wrapped pre-existing docs so full-repo
  `check_text.py --root .` passes with zero findings.
- Mandated `.editorconfig` and `docs-traceability-setup` `check_text.py` checks
  (line width, greedy prose packing, and Doxygen file headers) in `AGENTS.md`,
  `WDR_0002`, and `CNV_0002`, and wrapped `AGENTS.md` to 80 columns.
- Switched `authoring.modal_verb` in `.docs-traceability-setup.yml` from `shall`
  to `must` for ASD-STE100 Issue 9 compliance, revised all requirements in
  `docs/20-system/` to follow STE rules, expanded `technical_terms.txt` and
  `allowed_phrases.txt`, and made ASD-STE100 requirements compliance explicit in
  `AGENTS.md`, `WDR_0002`, and `CNV_0002`.
- Retargeted in-code `VER_` tags in `tests/test_runner.py`,
  `tools/traceability/check_needs_audit.py`, and `tests/traceability/` from
  `VER_POWER_*` to the real `BOOT`, `MEM`, `PROC`, `IPC`, `SIG`, `LIBC`, and
  `DOCS` verification items.
- The root `CMakeLists.txt` builds the documentation with
  `add_subdirectory(docs)` and enables CTest.
- The build container adds the `python3-venv` and `python3-pip` packages for the
  documentation venv.
- The headless QEMU test harness `tests/test_runner.py` carries the
  `VER_POWER_0005` traceability tag.

### Removed

- The `POWER` placeholder domain documents (`docs/*/power.md`) from the
  traceability model.

### Fixed

- Fixed `schedule()` idle loop in `kernel/core/sched.c` to enable interrupts
  (`sti; hlt; cli`), re-poll `pick_next()`, pop woken self-threads properly, and
  avoid recursive `sched_tick()` preemption when all threads are blocked.
- Reserved Multiboot2 boot modules (`bootfs`) in `pmm_init()` so dynamic frame
  allocations never overwrite the boot archive needed by `ProcCreate`.
- Fixed `ISR_STUB` in `kernel/arch/x86_64/isr.S` to pop all 16 bytes (`vec` and
  `err`) before `iretq` on error-code exceptions such as recoverable `#PF` COW
  page faults.
- Aligned secondary Ring 3 thread entry stacks in `sys_thread_reset` to satisfy
  the System V AMD64 `RSP % 16 == 8` C function entry invariant, preventing
  `#GP` faults on SSE `movaps` instructions in spawned worker threads.

### Added

- Scaffolding with the agent-scaffolding agent skill 0.2.0. The configuration is
  in `.agent-scaffolding.yml`.
- The MIT License in `LICENSE.md`, with copyright headers and SPDX identifiers
  in the source files.
- Governance and state documents: `AGENTS.md`, `docs/VISION.md`,
  `docs/DECISIONS.md`, `docs/CONVENTIONS.md`, `docs/THIRD-PARTY-NOTICES.md`, and
  this changelog.
- Agent skills in `docs/skills/`, with the Conventional Commits, SemVer, and
  Agent Skills specifications as references.
- A containerized development environment wrapper `scripts/enter-dev.sh`, and
  build and formatter agent scripts with their agent definitions in
  `docs/agents/`.
- Clang tool configurations `.clang-format` and `.clang-tidy`, verified in the
  build container.

### Changed

- The task plan moved from `TASKS.md` at the repository root to `docs/TASKS.md`.
- The test harness and the test applications moved to `tests/`:
  `tests/test_runner.py`, `tests/ipccli/`, and `tests/sigtest/`.
