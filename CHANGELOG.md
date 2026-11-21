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

### Changed

- Added Mermaid architecture, sequence, and state diagrams across `README.md`,
  `docs/00-overview/context.md`, and all domain architecture pages
  (`docs/30-architecture/{boot,mem,proc,ipc,sig,libc,docs}.md`) illustrating
  Ring 0/Ring 3 topology, boot bring-up, x86-64 memory map, COW faults, thread
  states, synchronous channel IPC, IRQ pulses, multi-server `ppoll()`, signal
  frames, three-layer POSIX `libc`, and bidirectional traceability.
- Added follow-up task `TSK_0021` (`SWR_BOOT_0003`) to `docs/records/TASKS.md`
  for ACPI `MADT`/`SPCR` hardware discovery, IOAPIC ISO routing, parameterized
  Ring 3 `ttyserv`/`rtcdrv`, and user `OPOST` TX ownership before final
  hardening (`TSK_0020`).
- Refined and dependency-ordered the remaining backlog in
  `docs/records/TASKS.md` (`TSK_0012` -> `TSK_0016` -> `TSK_0019` -> `TSK_0018`
  -> `TSK_0017` -> `TSK_0020`), adding draft requirements `SWR_LIBC_0007`
  (buffered `<stdio.h>`, `setenv`/`unsetenv`, `execvp`, core headers) and
  `SWR_DOCS_0004` (`<sys/select.h>` and cross-subsystem stress hardening).
- Removed volatile test and assertion counts from the `README.md` Mermaid
  architecture diagram and workflow overview so the diagram stays focused on
  tools and technologies.
- Included the root `README.md` dynamically as the top section of the generated
  Sphinx-Needs HTML home page (`docs/index.md` via
  `config/sphinx/readme_include.py`) with automatic Sphinx link rewriting and
  Mermaid diagram rendering.
- Updated `README.md` to document the closed-loop AI agent development cycle
  (Blueprint & bidirectional Sphinx-Needs traceability, Podman/LLVM Fabricator,
  headless QEMU Testbed, and automated Quality Gates) with a Mermaid flowchart.
- Extended and dependency-ordered the task backlog in `docs/records/TASKS.md`
  (`TSK_0013`, `TSK_0014`, `TSK_0015`, `TSK_0012`, `TSK_0016`, `TSK_0017`,
  `TSK_0018`) with matching draft requirements (`SWR_MEM_0006`, `SWR_IPC_0007`–
  `SWR_IPC_0009`, `SWR_LIBC_0006`).
- Consolidated all legacy `agent-scaffolding` documents (`docs/TASKS.md`,
  `docs/DECISIONS.md`, `docs/CONVENTIONS.md`, `docs/VISION.md`, and
  `docs/ARCHITECTURE.md`) into the Sphinx-Needs traceability model (`WDR_0003`–
  `WDR_0009`, `ADR_0004`–`ADR_0010`, `CNV_0005`–`CNV_0006`, `RDM_0004`–
  `RDM_0007`, `docs/00-overview/context.md`, and `docs/10-product/vision.md`),
  redirected `AGENTS.md`, `README.md`, and `docs/skills/` to the traceability
  records, retained `docs/THIRD-PARTY-NOTICES.md`, and removed the five legacy
  files (`TSK_0008`, `WDR_0009`).

### Added

- ACPI `MADT`/`SPCR` hardware discovery, parameterized Ring 3 drivers, Ring 3
  `OPOST`/`ECHO` TX, and interactive QEMU shell boot (`SWR_BOOT_0003`,
  `ADR_0022`, `COMP_BOOT_0003`, `VER_BOOT_0002`): added `kernel/core/acpi.c`
  parsing RSDP/RSDT/XSDT, `MADT` IOAPIC base and ISOs, and optional `SPCR`,
  ISO-aware IOAPIC setup in `kernel/core/intr.c`, `spawn_image_argv()` and
  `init=sh` interactive boot mode in `kernel/core/proc.c`, `-p`/`-i` driver
  arguments in `servers/ttyserv/main.c` and `servers/rtcdrv/main.c`, Ring 3
  16550A `OPOST`/`ONLCR`/`ECHO` TX routing via `/dev/tty` in `libc/src/tty.c`
  and `libc/src/fd.c`, `./scripts/env.sh run` interactive launcher, and user
  instructions in `README.md`.
- Ring 3 POSIX.1-2024 XCU shell (`sh`) and core utilities (`SWR_LIBC_0006`,
  `ADR_0021`, `COMP_LIBC_0005`, `VER_LIBC_0007`): added `bin/sh/main.c` (`-c`,
  script files, interactive `isatty()` prompt, pipelines over `pipe2`/`fork`/
  `dup2`/`execvp`, redirections `<`/`>`/`>>`/`2>`/`2>&1`, `&&`/`||`/`;`, `$VAR`/
  `$?`, Issue 8 `$'...'` ANSI-C quoting, `set -o pipefail`, `read -d` custom and
  NUL delimiters, built-ins `cd`/`pwd`/`export`/`unset`/`exit`), standalone
  `bin/utils/{echo,cat,ls,true,false,test}.c`, `bin` scanning in formatter and
  lint scripts, and QEMU `tests/shtest/test_sh.c`.
- Ring 3 `AF_UNIX` local sockets server and POSIX.1-2024 `accept4`
  (`SWR_IPC_0009`, `ADR_0020`, `ICD_IPC_0012`–`0013`, `COMP_IPC_0012`–`0013`,
  `VER_IPC_0007`): added `servers/sockserv/main.c` (`/dev/sock`) serving
  `AF_UNIX` `SOCK_STREAM` and `SOCK_DGRAM` sockets with deferred blocking
  `accept`/`send`/`recv`/`ppoll` queues, `FD_KIND_SOCK` close-pulse support in
  `kernel/core/proc.c`, standalone `<sys/socket.h>` and `<sys/un.h>`,
  async-signal-safe `libc/src/socket.c` (`socket`, `socketpair`, `bind`,
  `listen`, `accept`, `accept4` with atomic `SOCK_CLOEXEC`/`SOCK_NONBLOCK`,
  `connect`, `send`, `recv`, `sendto`, `recvfrom`, `shutdown`, `getsockname`,
  `getpeername`, `getsockopt`, `setsockopt`), multi-server `ppoll()` dispatch in
  `libc/src/fd.c`, and QEMU `tests/socktest/test_sock.c`.
- Buffered POSIX.1-2024 `<stdio.h>`, environment mutation, `execvp`, and core
  C17/POSIX headers (`SWR_LIBC_0007`, `ADR_0019`, `ICD_LIBC_0005`,
  `COMP_LIBC_0004`, `VER_LIBC_0006`): added standalone `<ctype.h>`,
  `<assert.h>`, `<inttypes.h>`, `<strings.h>`, and `<limits.h>` (with C17 and
  POSIX runtime limits), buffered `FILE` streams (`fopen`, `fdopen`, `fclose`,
  `fread`, `fwrite`, `fgets`, `fgetc`, `ungetc`, `fseeko`, `ftello`, `fileno`,
  `setvbuf`, `vsnprintf`, `vfprintf`, `perror`), `setenv`/`unsetenv`,
  `execv`/`execvp` with `/boot/` `bootfs` lookup, `strdup`/`strndup`/`strstr`,
  `strrchr`/`memchr`/`strtok_r`/`strerror`, and QEMU
  `tests/stdiotest/test_stdio.c`.
- Ring 3 serial TTY server and multi-server `ppoll()` (`SWR_IPC_0008`,
  `ADR_0018`, `ICD_IPC_0010`–`0011`, `COMP_IPC_0010`–`0011`, `VER_IPC_0006`):
  added `servers/ttyserv/main.c` (`/dev/tty`) driven by COM1 IRQ 4 IOAPIC pulses
  with canonical (`ICANON`, `VERASE`, `VKILL`, `ICRNL`, `VEOF`) and raw (`VMIN`)
  line discipline, standalone `<termios.h>`, `isatty`, async-signal-safe
  `libc/src/tty.c` (`tcgetattr`, `tcsetattr`, `tcflush`, `tcdrain`, `tcflow`,
  `tcsendbreak`, `cf*speed`), multi-server `ppoll()` routing in `libc/src/fd.c`,
  and QEMU `tests/ttytest/test_tty.c`.
- Option-gated POSIX.1-2024 memory extensions (`SWR_MEM_0005`, `ADR_0017`,
  `COMP_MEM_0005`, `VER_MEM_0004`): added `_POSIX_SHARED_MEMORY_OBJECTS`,
  `_POSIX_MEMLOCK`, and `_POSIX_MEMLOCK_RANGE` (`202405L`) in
  `libc/include/sys/_posix_opts.h`, kernel shared-frame `MAP_SHARED` and
  `SYS_SHM_OP`/`SYS_SHM_MAP`/`FD_KIND_SHM` in `kernel/core/shm.c`, `VMM_SHARED`
  and `VMM_LOCKED`/`SYS_VM_MLOCK` in `kernel/core/vmm.c`, `shm_open`,
  `shm_unlink`, `mlock`, `munlock`, `mlockall`, and `munlockall` in
  `libc/src/mman.c`, `ftruncate`/`fstat` SHM support in `libc/src/vfs.c`, and
  QEMU `tests/shmtest/test_shm.c`.
- Ring 3 POSIX VFS server (`SWR_IPC_0007`, `ADR_0016`, `ICD_IPC_0007`–`0009`,
  `COMP_IPC_0008`–`0009`, `VER_IPC_0005`): added `servers/vfsserv/main.c`
  (`/dev/vfs`) serving read-only `boot.tar` at `/boot` and writable dynamic
  `tmpfs` at `/` and `/tmp`, read-only `BOOTFS_USER_VA` mapping and
  `FD_KIND_VFS` close-pulse support in `kernel/core/proc.c`, `<sys/stat.h>`,
  `<dirent.h>`, `libc/src/vfs.c` (`open`, `openat`, `creat`, `lseek`,
  `ftruncate`, `stat`, `fstat`, `lstat`, `fstatat`, `mkdir`, `mkdirat`,
  `unlink`, `unlinkat`, `rmdir`, `chdir`, `fchdir`, `getcwd`, `umask`, `access`,
  `opendir`, `fdopendir`, `readdir`, `rewinddir`, `closedir`, `dirfd`), and QEMU
  `tests/vfstest/test_vfs.c`.
- Thread-safe `mmap`/`munmap`-backed heap allocator (`SWR_MEM_0006`, `ADR_0015`,
  `COMP_MEM_0004`, `VER_MEM_0003`): replaced the static 1 MiB bump arena in
  `libc/src/stdlib.c` with `libc/src/malloc.c` featuring a `SYS_FUTEX` lock, 64
  KiB coalescing slabs with whole-slab `munmap` reclamation, direct
  `mmap`/`munmap` for allocations >= 32 KiB, in-place `realloc` growth and
  splitting, POSIX.1-2024 `errno`-preserving `free`, and QEMU
  `tests/heaptest/test_heap.c`.
- Complete 4-gate POSIX.1-2024 conformance harness (`SWR_DOCS_0003`,
  `COMP_DOCS_0003`, `VER_DOCS_0004`): added CTest Gate 2 `posix.symbol_diff`
  (`tests/posix/test_symbol_diff.py` comparing Clang JSON AST declarations and
  `llvm-nm` `libc.a` exports against `tests/posix/issue8-manifest.tsv` with diff
  report generation) and Gate 3b `posix.cancellation`
  (`tests/posix/test_cancellation.py` auditing XSH 2.9.5 cancellation call-graph
  reachability), expanded `posix.async_signal_safe` forbidden callees to include
  `realloc*`, `<stdio.h>` stream locks, and mutex locks, and centralized
  call-graph and manifest helpers in `tools/posix/audit_lib.py`.
- POSIX.1-2024 Base library functions and C17 headers (`SWR_LIBC_0005`,
  `ADR_0014`): kernel `SYS_GETENTROPY` (`48`) mixer (`kernel/core/entropy.c`),
  `getentropy` (`GETENTROPY_MAX = 256`), `strlcpy`, `strlcat`, `realloc`,
  `reallocarray`, `_Exit`, `getenv`, `secure_getenv`, `<uchar.h>` UTF-16/UTF-32
  conversions (`libc/src/uchar.c`), standalone `<stdatomic.h>`/`<stdalign.h>`
  verification, and QEMU `tests/libctest/test_libc_issue8.c`.
- POSIX.1-2024 Base thread cancellation points and recursive `<stdio.h>` stream
  locking (`SWR_LIBC_0004`, `ADR_0013`): `pthread_cancel`,
  `pthread_setcancelstate`, `pthread_setcanceltype`, `pthread_testcancel`,
  `pthread_cleanup_push`/`pop`, internal `LIBC_SIGCANCEL` wakeup across
  `pthread_cond_wait`, `read`, `write`, `close`, `ppoll`, `sem_wait`,
  `clock_nanosleep`, `waitpid`, `aio_suspend`, and `pthread_join`, plus `FILE`
  (`stdin`/`stdout`/`stderr`), `flockfile`, `ftrylockfile`, `funlockfile`,
  `fprintf`, `fputs`, `fputc`, `putchar_unlocked`, `fflush`, and QEMU
  `tests/canceltest`.
- POSIX.1-2024 Base asynchronous I/O (`SWR_IPC_0005`, `ADR_0012`): `<aio.h>`,
  `O_SYNC`/`O_DSYNC` in `<fcntl.h>`, Ring 3 worker-thread engine
  (`libc/src/aio.c`) with `aio_read`, `aio_write`, `aio_fsync`, `aio_cancel`,
  `lio_listio`, `SIGEV_SIGNAL`, async-signal-safe `aio_error`/`aio_return`/
  `aio_suspend`, QEMU `tests/aiotest`, and CTest header/safety coverage.
- POSIX.1-2024 Base `fork`, `execve`, `pipe2`, `dup3`, `fcntl`, `poll`, `ppoll`,
  and `sigpending` (`TSK_0007`, `SWR_PROC_0004`, `SWR_IPC_0004`): syscalls
  `SYS_PROC_FORK` (`42`), `SYS_PROC_EXECVE` (`43`), `SYS_PROC_GETPID` (`44`),
  `SYS_PROC_GETPPID` (`45`), `SYS_FD_OP` (`46`), and `SYS_SIGPENDING` (`47`),
  kernel per-process FD metadata with `PULSE_CODE_FD_CLOSE` cleanup, Ring 3
  `servers/pipeserv` (`/dev/pipe`), `<fcntl.h>`, `<poll.h>`, async-signal-safe
  `libc/src/fd.c`, QEMU `tests/forkpipetest`/`exechelper`, and CTest coverage
  (`ADR_0011`).
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

- Added `${CMAKE_SOURCE_DIR}/kernel` to `kernel.elf` include directories in
  `CMakeLists.txt` and replaced all parent-relative `../arch/x86_64/` includes
  with `<arch/x86_64/...>` across the kernel.
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
