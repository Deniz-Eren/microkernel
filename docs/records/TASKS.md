# Tasks

## Purpose

Track active, temporary engineering tasks (`task`) for `microkernel`.

## Structure

Tasks are ordered by optimal execution order (highest priority first) and link
into the normative model through `:traces:`.

## Formatting Rules

- Normative specifications and records must not link to `TSK_` items.
- Move completed or cancelled tasks verbatim into [HISTORY.md](HISTORY.md) with
  status `done` or `cancelled`.

```{needtable} All tasks
:types: task
:filter: docname == 'records/TASKS'
:columns: id; title; status; traces as "Traces to"; traces_back as "Traced by"
```

### TSK_0055 — Add host-to-guest networking, netserv hardening, nc, and ping

```{task} Add host-to-guest networking, netserv hardening, nc, and ping
:id: TSK_0055
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0042, SYS_IPC_0001, SYS_LIBC_0001

Add localhost QEMU and Podman port forwarding in `scripts/env.sh` and harden
`servers/netserv/` for inbound host TCP traffic. Add Ring 3 `nc` and `ping`
utilities with an automated host-to-guest TCP test ({need}`ADR_0042`,
{need}`SYS_IPC_0001`, {need}`SYS_LIBC_0001`).
```

### TSK_0056 — Add Ring 3 PTYs, virtual devices, SCM_RIGHTS, and OpenSSH APIs

```{task} Add Ring 3 PTYs, virtual devices, SCM_RIGHTS, and OpenSSH APIs
:id: TSK_0056
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_IPC_0001, SYS_LIBC_0001

Extend `servers/ttyserv/` with Ring 3 PTYs (`/dev/ptmx`, `/dev/pts/N`) and
`ioctl` support. Add `/dev/null`, `/dev/urandom`, Ring 3 `SCM_RIGHTS` FD
passing, `<syslog.h>`, `crypt`, and OpenSSH `libc` APIs with QEMU tests
({need}`SYS_IPC_0001`, {need}`SYS_LIBC_0001`, {need}`SYS_PROC_0001`).
```

### TSK_0057 — Port OpenSSH Portable sshd and ssh-keygen to Ring 3

```{task} Port OpenSSH Portable sshd and ssh-keygen to Ring 3
:id: TSK_0057
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_IPC_0001, SYS_LIBC_0001

Cross-compile OpenSSH Portable (`--without-openssl --without-zlib`) for Ring 3
with Ed25519 `authorized_keys` and dev password login. Start `sshd` on
interactive boot and verify real OpenSSH client logins in CTest
({need}`SYS_IPC_0001`, {need}`SYS_LIBC_0001`).
```
