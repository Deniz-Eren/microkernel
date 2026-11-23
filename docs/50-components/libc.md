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

```{comp} POSIX.1-2024 floating-point environment and x86-64-v3 math library
:id: COMP_LIBC_0007
:status: active
:posix: XBD/fenv.h (CX), XBD/math.h (CX)
:refines: ARC_LIBC_0001, ICD_LIBC_0007
:satisfies: SWR_LIBC_0009

`libc/arch/x86-64/fenv.c` implements `<fenv.h>` rounding and exception flags
through `MXCSR` and x87 control words. `libc/src/math_{basic,round,exp,trig}.c`
implements `<math.h>` classification, hardware `FMA`/`sqrt`/`vround`,
exponential, logarithmic, power, error, gamma, trigonometric, and hyperbolic
functions ({need}`ICD_LIBC_0007`, {need}`SWR_LIBC_0009`).
```

```{comp} ISO C17 <threads.h> and Base signal, waitid, and semaphore support
:id: COMP_LIBC_0008
:status: active
:posix: XBD/threads.h (CX), XSH/waitid (BASE), XSH/sem_open (BASE)
:refines: ARC_LIBC_0001, ICD_LIBC_0008
:satisfies: SWR_PROC_0012

`libc/src/threads.c` implements `<threads.h>` over `pthread_*`. `signal.c`,
`wait.c`, `unistd.c`, `stdio.c`, and `stdlib.c` implement Base signal waits,
`waitid`, `_Fork`, `fexecve`, `popen`/`pclose`, and `system`. `pthread.c`,
`sched.c`, and `semaphore.c` implement robust mutexes, LIFO `pthread_atfork`,
`sched_*`, and `MAP_SHARED` named semaphores ({need}`ICD_LIBC_0008`,
{need}`SWR_PROC_0012`).
```

```{comp} Base stdio memory/scan streams, strtod*, aligned heap, and time
:id: COMP_LIBC_0009
:status: active
:posix: XBD/stdio.h (CX), XBD/stdlib.h (CX), XBD/string.h (CX), XBD/time.h (CX)
:refines: ARC_LIBC_0001, ICD_LIBC_0009
:satisfies: SWR_LIBC_0010

`libc/src/stdio.c` and `stdio_scanf.c` implement `scanf*`, `getline`/`getdelim`,
`dprintf*`, `asprintf*`, `fmemopen`, `open_memstream`, `freopen`, and
`fgetpos`/`fsetpos`. `stdlib.c`, `stdlib_float.c`, `malloc.c`, and `string.c`
implement `strtod*`, `atof`, `aligned_alloc`, `quick_exit`/`at_quick_exit`,
`div*`, `rand`/`srand`, `qsort_r`, `getsubopt`, `stpcpy`, `stpncpy`, `memmem`,
and `strerror_r`. `time.c` and `time_cal.c` implement `time`, `difftime`,
`clock`, `timespec_get`, `gmtime*`, `localtime*`, `mktime`, `asctime*`,
`ctime*`, and `strftime` ({need}`ICD_LIBC_0009`, {need}`SWR_LIBC_0010`).
```

```{comp} Complex math library and Annex G complex multiply/divide helpers
:id: COMP_LIBC_0010
:status: active
:posix: XBD/complex.h (CX), XBD/tgmath.h (CX)
:refines: ARC_LIBC_0001, ICD_LIBC_0010
:satisfies: SWR_LIBC_0011

`libc/src/complex.c` implements all 66 `<complex.h>` manipulation, power,
exponential, logarithmic, trigonometric, and hyperbolic functions over
`<math.h>`. `libc/src/complex_arith.c` supplies Annex G `__mulsc3`, `__muldc3`,
`__mulxc3`, `__divsc3`, `__divdc3`, and `__divxc3` runtime helpers
({need}`ICD_LIBC_0010`, {need}`SWR_LIBC_0011`).
```

```{comp} Ring 3 file timestamp, lgamma*, and timezone state components
:id: COMP_LIBC_0011
:status: active
:posix: XBD/sys_stat.h (CX), XBD/math.h (CX), XBD/time.h (CX)
:refines: ARC_LIBC_0001, ICD_LIBC_0011
:satisfies: SWR_LIBC_0012

`libc/src/vfs.c`, `servers/vfsserv/main.c`, and `bin/utils/touch.c` implement
`futimens`, `utimensat`, and `VFS_OP_UTIMENS` with `ext2` persistence.
`libc/src/math_exp.c` implements Lanczos `lgamma`, `lgammaf`, and `lgammal`.
`libc/src/time_cal.c` implements `tzset` and `tzname[2]` over `TZ`
({need}`ICD_LIBC_0011`, {need}`SWR_LIBC_0012`).
```

```{comp} Ring 3 locale, multibyte, wctype, and core wchar components
:id: COMP_LIBC_0012
:status: active
:posix: XBD/locale.h (CX), XBD/wchar.h (CX), XBD/wctype.h (CX)
:refines: ARC_LIBC_0001, ICD_LIBC_0012
:satisfies: SWR_LIBC_0013

`libc/src/locale.c` implements `C`/`C.UTF-8` locale management over `FS.base`
TLS. `libc/src/ctype.c`, `string.c`, and `time_cal.c` implement `*_l` helpers,
`strcoll*`, and `strxfrm*`. `libc/src/wctype.c`, `wchar_mb.c`, `wchar_str.c`,
and `wchar_conv.c` implement `<wctype.h>`, UTF-8 multibyte conversions, `wmem*`,
`wcs*`, `wcsto*`, `wcstoimax`/`wcstoumax`, and `wcsftime`
({need}`ICD_LIBC_0012`, {need}`SWR_LIBC_0013`).
```
