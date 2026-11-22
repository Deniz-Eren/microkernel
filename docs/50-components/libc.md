# Components — LIBC

Domain `LIBC`: White-box specifications for the Ring 3 C runtime and
thread-local `errno` storage.

```{comp} Freestanding crt0, core C helpers, and syscall wrappers
:id: COMP_LIBC_0001
:status: active
:refines: ARC_LIBC_0001, ICD_LIBC_0001, ICD_LIBC_0002
:satisfies: SWR_LIBC_0001, SWR_LIBC_0003, SWR_LIBC_0004

`libc/arch/x86-64/crt0.S` and `libc/src/` (`start.c`, `stdio.c`, `stdlib.c`,
`string.c`, `unistd.c`) give Ring 3 startup, `__sigreturn_trampoline`, recursive
`<stdio.h>` stream locking, heap, string helpers, and `errno` wrappers
({need}`ICD_LIBC_0001`, {need}`ICD_LIBC_0002`, {need}`ARC_LIBC_0001`).
```

```{comp} Lock-free TID-indexed thread-local errno storage
:id: COMP_LIBC_0002
:status: active
:refines: ARC_LIBC_0001, ICD_LIBC_0003
:satisfies: SWR_LIBC_0003

`libc/src/errno.c` puts `__errno_location()` on a 64-slot atomic table keyed by
kernel TID. It gives lock-free, async-signal-safe `errno` for each thread
({need}`ICD_LIBC_0003`, {need}`ARC_LIBC_0001`).
```

```{comp} ISO C17 and POSIX.1-2024 Unicode conversion and Base extensions
:id: COMP_LIBC_0003
:status: active
:posix: XSH/getentropy (BASE), XSH/strlcpy (CX), XBD/uchar.h (CX)
:refines: ARC_LIBC_0001, ICD_LIBC_0004
:satisfies: SWR_LIBC_0005

`libc/src/uchar.c` converts UTF-8 to and from UTF-16/UTF-32 with surrogate-pair
handling and `EILSEQ` checks. `string.c`, `stdlib.c`, and `unistd.c` provide
`strlcpy`, `strlcat`, `reallocarray`, `secure_getenv`, and `getentropy` over
`SYS_GETENTROPY` ({need}`ICD_LIBC_0004`, {need}`SWR_LIBC_0005`).
```

```{comp} Buffered stdio streams, env mutation, execvp, and C17 helpers
:id: COMP_LIBC_0004
:status: active
:posix: XBD/ctype.h (CX), XSH/fopen (CX), XSH/setenv (BASE), XSH/exec (BASE)
:refines: ARC_LIBC_0001, ICD_LIBC_0005
:satisfies: SWR_LIBC_0007

`libc/src/stdio.c` provides buffered `FILE` I/O (`fopen`, `fdopen`, `fclose`,
`fread`, `fwrite`, `fgets`, `fseeko`, `ftello`, `ungetc`, `vsnprintf`, `perror`)
with mutex-protected stream flushing. `ctype.c`, `stdlib.c`, `string.c`, and
`unistd.c` implement `<ctype.h>`, `<strings.h>`, `<inttypes.h>`,
`__assert_fail`, `setenv`, `unsetenv`, `execv`, `execvp`, `strdup`, `strstr`,
and `strerror` ({need}`ICD_LIBC_0005`, {need}`SWR_LIBC_0007`).
```

```{comp} Ring 3 command shell and core utilities targeting POSIX.1-2024 XCU
:id: COMP_LIBC_0005
:status: active
:posix: XCU/2 (BASE), XCU/read (BASE), XCU/set (BASE)
:refines: ARC_LIBC_0001, ICD_LIBC_0005
:satisfies: SWR_LIBC_0006

`bin/sh/main.c` provides a Ring 3 command shell (`sh`) with `-c`, script files,
interactive `isatty()` input, pipelines, redirections, `$'...'` quoting, `set -o
pipefail`, and `read -d`. `bin/utils/` provides `echo`, `cat`, `ls`, `true`,
`false`, and `test` ({need}`ICD_LIBC_0005`, {need}`SWR_LIBC_0006`).
```

```{comp} POSIX.1-2024 Base system utilities, setjmp, fnmatch, and getopt
:id: COMP_LIBC_0006
:status: active
:posix: XBD/sys/utsname.h (BASE), XBD/endian.h (BASE), XBD/sched.h (BASE)
:refines: ARC_LIBC_0001, ICD_LIBC_0006
:satisfies: SWR_LIBC_0008

`libc/src/` (`utsname.c`, `endian.c`, `sched.c`, `setjmp.c`, `fnmatch.c`,
`getopt.c`, `stdlib.c`, `time.c`, `vfs.c`) and `libc/arch/x86-64/setjmp.S`
supply `uname`, byte-order conversions, `sched_yield`, `setjmp`/`sigsetjmp`,
`times`, `fnmatch`, `atexit`, `qsort`, `bsearch`, `getopt`, `alarm`, `rename`,
`renameat`, and `realpath` ({need}`ICD_LIBC_0006`, {need}`SWR_LIBC_0008`).
```
