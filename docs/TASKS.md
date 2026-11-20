# Microkernel task plan

The project moves forward in milestones. Each milestone adds one group of functions. A milestone is finished only when its automated test gives exit code 0. The test harness compares the serial output of the system with text patterns.

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
- `SendMsg`, `ReceiveMsg`, and `ReplyMsg`. A sender waits until the server receives the message, and then until the reply comes.
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

## Milestone 5 — processes and memory system calls (next)

- `ProcCreate`: a process makes a new process from a program in the boot archive
- `VMMap` and `VMUnmap`: user processes change their own memory map
- `VMCloneCow`: copy-on-write pages between related processes
- the parent process can read the exit code of a child process
- the scheduler runs the processes of all milestones together
- new tests for process creation and for the memory system calls

## Milestone 6 — interrupts and drivers (planned)

- interrupt events as messages to user processes (`AttachIntrEvent`)
- access to port input and output for permitted processes (`IOPLSet`)
- a driver for a QEMU device, with a message path that uses interrupts
- `ReadMsgV` and `WriteMsgV` for messages with more than one buffer

## Work rules

- Make one milestone at a time.
- Do not start a milestone before the test of the last milestone gives exit code 0.
- Make a commit after each completed milestone.
- When you repair a defect, record the repair in the commit message.
