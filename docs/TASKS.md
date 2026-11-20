# Microkernel task plan

The project moves forward in milestones. Each milestone adds one group of
functions. A milestone is finished only when its automated test gives exit code
0. The test harness compares the serial output of the system with text patterns.

## Purpose

Record all the tasks of microkernel. Give the status and the date for each task.

## Structure

The tasks are in groups by milestone and by category. Each task is a checkbox
with its status and date.

## Formatting Rules

- Use `- [ ] <type>: <description>` for open tasks.
- Use `- [x] COMPLETE YYYY-MM-DD <type>: <description>` for complete tasks.
- Do not remove completed tasks.

## Milestone 1 — boot and memory (completed)

- build environment in a Podman container, with Clang and CMake
- boot with GRUB and the Multiboot2 protocol
- kernel at a high virtual address, with boot code in 32-bit and 64-bit mode
- physical memory manager with a bitmap
- virtual memory manager with page tables and the W^X policy
- GDT, IDT, and the LAPIC timer with calibration from the PIT
- test harness for headless QEMU with serial output checks

## Milestone 2 — user processes (completed)

- four-stage build: headers, C library, user programs, kernel and ISO
- small C library with `printf`, a memory allocator, and system call wrappers
- system call entry with the `syscall`/`sysret` instructions
- the scheduler with ready queues and time slices
- the ELF loader for user programs, with page-level permissions
- the first user process `sysproc`, started from the `boot.tar` archive
- verified: entry into Ring 3, and the first system calls

## Milestone 3 — messages between processes (completed)

- channels with priority-ordered queues for messages, receivers, and pulses
- `SendMsg`, `ReceiveMsg`, and `ReplyMsg`. A sender waits until the server
  receives the message, and then until the reply comes.
- priority inheritance for the server while it processes a message
- connections between processes, with `OpenConnection` and `CloseConnection`
- data moves between address spaces with page permission checks
- `sysproc` as the name registry: servers register names, clients find them
- `ipcserv` and `ipccli`: a demo echo server and its client
- verified: the name lookup, three messages with replies, and a clean exit

## Milestone 4 — threads and signals (completed)

- `NewThread` makes an embryo thread, `ThreadReset` arms and starts it
- `ThreadGetCtx` gives the state of a thread
- kernel-allocated user stacks for new threads
- signal handlers in Ring 3, with a kernel-built signal frame on the user stack
- `kill` sends a signal. Threads blocked in messages wake with `EINTR`
- a signal with no handler stops the process
- `sigtest`: a worker thread, a caught signal, and a signal with no handler
- verified: a thread starts, a signal handler runs, and the process stops

## Milestone 5 — processes and memory system calls (completed)

- `ProcCreate`: a process makes a new process from a program in the boot archive
- `VMMap`, `VMUnmap`, and `VMProtect`: user processes change their own memory
  map with W^X enforcement (`<sys/mman.h>` `mmap`, `mprotect`, `msync`,
  `munmap`)
- `VMCloneCow`: copy-on-write pages between related processes with per-frame PMM
  refcounting and Ring 3 `#PF` COW fault resolution
- `ProcWait` / `<sys/wait.h>` `waitpid`: the parent process reads child exit
  codes and signal status (`WNOHANG`, blocking wait, PML4 reclaim on reap)
- the scheduler runs the processes of all milestones (1 to 5) together
- verified: `proctest` and `procchild` pass in headless QEMU alongside CTest
  `posix.headers` and `posix.async_signal_safe`

## Milestone 6 — interrupts and drivers (next)

- [ ] feat(ipc,drivers): TSK_0004 — add `AttachIntrEvent`, `IntrUnmask`,
  `IOPLSet`, and `ReadMsgV`/`WriteMsgV`. Run a Ring 3 QEMU device driver test.

## Near-term POSIX.1-2024 tasks (planned)

- [ ] feat(proc,time): TSK_0005 — add kernel monotonic/realtime clocks and
  POSIX.1-2024 `<time.h>` (`clock_gettime`, `clock_settime`, `clock_nanosleep`,
  `timer_create`, `timer_settime`) for `CLOCK_MONOTONIC` and `CLOCK_REALTIME`
- [ ] feat(proc,pthread): TSK_0006 — add per-thread `FS.base` TLS, `<pthread.h>`
  (`pthread_create`, `pthread_join`, mutexes, `pthread_cond_*`,
  `pthread_cond_clockwait`), and `<semaphore.h>` (`sem_*`)
- [ ] feat(proc,ipc): TSK_0007 — add `fork` (on full `VMCloneCow`), `execve`
  (from `boot.tar`), per-process file descriptors, and `pipe2`/`dup3`/`ppoll`

For the full traceability backlog and longer-term roadmap, see
[docs/records/TASKS.md](records/TASKS.md) and
[docs/records/ROADMAP.md](records/ROADMAP.md).

## Work rules

- Make one milestone at a time.
- Do not start a milestone before the test of the last milestone gives exit code
  0.
- Make a commit after each completed milestone.
- When you repair a defect, record the repair in the commit message.

## Scaffolding

- [x] COMPLETE 2026-11-20 scaffold: apply agent-scaffolding 0.2.0 (governance,
  state documents, skills, tests/ restructure, license, tool configs, build
  agents, copyright headers)
- [x] COMPLETE 2026-11-20 scaffold: apply docs-traceability-setup 0.1.0 (Sphinx,
  MyST-Parser, Sphinx-Needs, CMake/CTest traceability model)
- [x] COMPLETE 2026-11-21 docs: replace POWER placeholder domain with real
  microkernel domains (BOOT, MEM, PROC, IPC, SIG, LIBC, DOCS) prepared for
  POSIX.1-2024 alignment
- [x] COMPLETE 2026-11-21 docs: refine traceability requirements against
  POSIX.1-2024 (Issue 8) Base using posix-issue8-os-dev
- [x] COMPLETE 2026-11-21 feat(libc): complete TSK_0001 (POSIX.1-2024 header
  hygiene, thread-local errno, and posix.headers standalone CTest gate)
- [x] COMPLETE 2026-11-21 docs(traceability): establish two-tier bidirectional
  code and POSIX.1-2024 traceability (WDR_0002, :posix: matrix, need_tag_check
  coverage gate, full codebase ICD_/COMP_ tags, and AGENTS.md rules)
- [x] COMPLETE 2026-11-21 docs(ste): align all requirements documents to
  ASD-STE100 Issue 9 (modal_verb=must, glossaries, AGENTS.md, CNV_0002, ADR-12)
- [x] COMPLETE 2026-11-21 docs(governance): mandate .editorconfig and
  docs-traceability-setup check_text.py checks in AGENTS.md, WDR_0002, and
  CNV_0002
- [x] COMPLETE 2026-11-21 style: bring full repository to zero check_text.py
  findings (80-column .clang-format, Doxygen headers, greedy prose packing,
  immutable .editorconfig exemptions, ADR-13)
- [x] COMPLETE 2026-11-21 docs(readme): document POSIX.1-2024 (Issue 8) Base
  compliance target and traceability links in README.md
- [x] COMPLETE 2026-11-21 feat(sig): complete TSK_0003 (POSIX.1-2024 <signal.h>,
  SYS_SIGPROCMASK, SIGKILL/SIGSTOP EINVAL, sig_frame_t mask restore, and libc.a
  async-signal-safety CTest gate)
- [x] COMPLETE 2026-11-21 docs(readme): add user-space support breakdown and
  bidirectional traceability overview to README.md and mandate README sync in
  AGENTS.md
- [x] COMPLETE 2026-11-21 feat(proc,mem): complete TSK_0002 / Milestone 5
  (ProcCreate, ProcWait/waitpid, VMMap/VMUnmap/VMProtect/mmap, VMCloneCow with
  PMM refcounts and #PF COW resolution, proctest/procchild QEMU & CTest gates)
- [x] COMPLETE 2026-11-21 docs(tasks): add near-term forward task backlog
  (TSK_0004-TSK_0007) in docs/TASKS.md and docs/records/TASKS.md and archive
  RDM_0001
- [x] COMPLETE 2026-11-21 docs(glossary): explain Ring 0, Ring 3, x86-64 memory
  protection, COW, IPC, and microkernel build terms in
  docs/00-overview/glossary.md
