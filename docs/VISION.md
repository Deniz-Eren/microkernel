## Purpose

Define the long-term vision and the principles of microkernel.

## Structure

The document has the mission, the principles, and the target result.

## Formatting Rules

- Use short and clear language.
- Look at the vision again and update it when the direction of the project changes. Record the change as an ADR in [DECISIONS.md](DECISIONS.md).

## Mission

A small operating system for x86-64 computers. The kernel is small. It supplies its services to user processes. Servers and applications run as user processes in Ring 3. The processes send messages to each other.

## Principles

- **Small kernel:** the kernel gives only scheduling, messages, threads, signals, and memory management. All other services run in user space.
- **Always correct:** a milestone is complete only when its automated test gives exit code 0.
- **Memory safety:** the system maps a page as writable or as code, but not as the two together (W^X). The kernel examines page permissions before each copy through user pointers.
- **Single source of truth:** each procedure is in one skill file.
- **Traceable:** each decision is an ADR, and each AI-assisted commit has the tag `Assisted-by:`.

## Target Result

The project is complete when all milestones in [TASKS.md](TASKS.md) are complete:

- the kernel boots, and the memory manager starts
- user processes run in Ring 3, with system calls, messages, threads, and signals
- processes make new processes, and user processes manage their memory maps
- interrupt events reach user processes as messages, with a driver for a QEMU device
- the full test of all milestones gives exit code 0 in one run
