# Roadmap

## Purpose

Track planned milestones and roadmap entries (`roadmap`) for `microkernel`.

## Structure

Order roadmap items by priority and link them to affected specifications through
`:traces:`.

## Formatting Rules

- Normative specifications and records must not link to `RDM_` items.
- Move completed or cancelled roadmap items verbatim into
  [HISTORY.md](HISTORY.md) with status `done` or `cancelled`.

```{needtable} All roadmap items
:types: roadmap
:filter: docname == 'records/ROADMAP'
:columns: id; title; status; traces as "Traces to"; traces_back as "Traced by"
```

### RDM_0001 — Milestone 5: Processes and memory system calls

```{roadmap} Milestone 5: Processes and memory system calls
:id: RDM_0001
:status: open
:traces: SYS_PROC_0001, SYS_MEM_0001

Add user-space process spawning (`ProcCreate`), virtual memory mapping (`VMMap`,
`VMUnmap`), copy-on-write cloning (`VMCloneCow`), and parent exit-code
collection ({need}`SYS_PROC_0001`, {need}`SYS_MEM_0001`).
```

### RDM_0002 — Milestone 6: Interrupts and user-space drivers

```{roadmap} Milestone 6: Interrupts and user-space drivers
:id: RDM_0002
:status: open
:traces: SYS_IPC_0001

Deliver hardware interrupt events as IPC messages (`AttachIntrEvent`), support
`IOPLSet` and multi-buffer `ReadMsgV`/`WriteMsgV`, and run a Ring 3 QEMU device
driver ({need}`SYS_IPC_0001`).
```

### RDM_0003 — POSIX.1-2024 (Issue 8) Base conformance

```{roadmap} POSIX.1-2024 (Issue 8) Base conformance
:id: RDM_0003
:status: open
:traces: BDR_0003, SYS_LIBC_0001, SYS_PROC_0001, SYS_SIG_0001

Align headers, `errno`, threads, signals, memory mapping, and core utilities to
IEEE Std 1003.1-2024 Base requirements ({need}`BDR_0003`, {need}`SYS_LIBC_0001`,
{need}`SYS_PROC_0001`, {need}`SYS_SIG_0001`).
```
