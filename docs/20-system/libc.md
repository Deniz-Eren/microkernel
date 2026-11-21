# System and Software Requirements — LIBC

Domain `LIBC`: freestanding Ring 3 C17 runtime, public headers, syscall
wrappers, and POSIX.1-2024 Issue 8 Base rules.

```{sys} Supply a freestanding C17 library aligned to POSIX.1-2024
:id: SYS_LIBC_0001
:status: active

The system must supply a freestanding Ring 3 C17 library and public header set.
These interfaces must operate for user programs and obey POSIX.1-2024 Issue 8
Base rules.
```

```{swr} Supply freestanding crt0, C helpers, and syscall wrappers
:id: SWR_LIBC_0001
:status: active
:derives: SYS_LIBC_0001

The C library must supply `crt0`, the signal trampoline, console output, string
and heap helpers, and syscall wrappers. All C library code must compile with
`-ffreestanding -Wall -Wextra -Werror` ({need}`SYS_LIBC_0001`).
```

```{swr} Keep Issue 8 headers standalone with clean namespaces
:id: SWR_LIBC_0002
:status: active
:posix: XBD/unistd.h (BASE), XBD/13-14, XSH/2.2
:derives: SYS_LIBC_0001
:traces: BDR_0003, ADR_0003

Each POSIX.1-2024 header must compile alone with `-std=c17 -Wall -Wextra -Werror
-pedantic`, `-D_POSIX_C_SOURCE=202405L`, and `-D_XOPEN_SOURCE=800` ([XBD
13][xbd-13], [XBD 14][xbd-14], [XSH 2.2][xsh-02]). `<unistd.h>`
([unistd.h][hdr-unistd]) must set `_POSIX_VERSION` and `_POSIX2_VERSION` to
`202405L` and not include `<sys/microkernel.h>`. It must keep internal names in
`__` or `_[A-Z]` and put option groups behind `_POSIX_*` macros
({need}`SYS_LIBC_0001`).
```

```{swr} Supply thread-local errno isolated from raw syscalls
:id: SWR_LIBC_0003
:status: active
:posix: XBD/errno.h (CX), XSH/2.3
:derives: SYS_LIBC_0001
:traces: BDR_0003, ADR_0003

`<errno.h>` ([errno.h][hdr-errno]) must declare thread-local `errno` to obey
[XSH 2.3][xsh-02]. Libc wrappers must isolate raw syscall error codes from
`errno` and set `errno` only on failure. Wrappers must not clear `errno` on
success and must obey the mandatory error sequence ({need}`SYS_LIBC_0001`).
```

```{swr} Supply mandatory POSIX.1-2024 thread cancellation points
:id: SWR_LIBC_0004
:status: active
:posix: XSH/2.9.5 (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, ADR_0003, ADR_0013

Libc wrappers for mandatory cancellation points (`read`, `write`, `open`,
`close`, `nanosleep`, `clock_nanosleep`, `ppoll`, `pthread_cond_wait`,
`pthread_cond_clockwait`, `sem_wait`, `waitpid`, and the full [XSH 2.9][xsh-02]
set) must examine deferred cancellation at safe points with no locks held
({need}`SYS_LIBC_0001`).
```

```{swr} Supply new POSIX.1-2024 Base functions and C17/C11 headers
:id: SWR_LIBC_0005
:status: active
:posix: XSH/getentropy (BASE), XSH/strlcpy (CX), XBD/stdatomic.h (CX)
:derives: SYS_LIBC_0001
:traces: BDR_0003, ADR_0003, ADR_0014

With `-D_POSIX_C_SOURCE=202405L`, `libc` must declare new Issue 8 Base functions
`strlcpy`, `strlcat`, `reallocarray`, `getentropy` ([getentropy][fn-getent]),
and `secure_getenv`. It must also supply `<stdatomic.h>`, `<stdalign.h>`, and
`<uchar.h>` ({need}`SYS_LIBC_0001`).
```

```{swr} Supply Ring 3 POSIX.1-2024 XCU shell utility
:id: SWR_LIBC_0006
:status: draft
:posix: XCU/2 (BASE), XCU/read (BASE), XCU/set (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, ADR_0003

A Ring 3 POSIX shell (`sh`) and minimal core utilities must execute pipelines
and redirections over `fork`/`execvp`/`pipe2`/`dup2`. The shell must support
Issue 8 `$'...'` quoting, `set -o pipefail`, and `read -d`
({need}`SYS_LIBC_0001`).
```

```{swr} Supply POSIX.1-2024 buffered stdio, env, execvp, and core headers
:id: SWR_LIBC_0007
:status: draft
:posix: XBD/ctype.h (CX), XSH/fopen (CX), XSH/setenv (BASE), XSH/exec (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, ADR_0003

`libc` must supply `<ctype.h>`, `<assert.h>`, `<inttypes.h>`, `<strings.h>`, and
`<limits.h>` with POSIX limits. It must also supply buffered `<stdio.h>`
(`fopen`, `fdopen`, `fclose`, `fread`, `fwrite`, `fgets`, `fseek`, `fileno`),
`setenv`, `unsetenv`, `execv`, `execvp`, `strdup`, `strstr`, and `strerror`
({need}`SYS_LIBC_0001`).
```

[xbd-13]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/V1_chap13.html
[xbd-14]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/V1_chap14.html
[xsh-02]:
  https://pubs.opengroup.org/onlinepubs/9799919799/functions/V2_chap02.html
[hdr-unistd]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/unistd.h.html
[hdr-errno]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/errno.h.html
[fn-getent]:
  https://pubs.opengroup.org/onlinepubs/9799919799/functions/getentropy.html
