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

```{comp} Ring 3 wide-oriented stdio, *wprintf, *wscanf, and open_wmemstream
:id: COMP_LIBC_0013
:status: active
:posix: XBD/wchar.h (CX), XBD/stdio.h (CX)
:refines: ARC_LIBC_0001, ICD_LIBC_0013
:satisfies: SWR_LIBC_0014

`libc/src/stdio.c` implements stream orientation (`fwide`), wide character I/O
(`fgetwc*`, `fputwc*`, `getwc*`, `putwc*`, `ungetwc`), and `open_wmemstream`.
`libc/src/wchar_printf.c` implements `fwprintf`, `wprintf`, `swprintf`, and
`v*wprintf`. `libc/src/wchar_scanf.c` implements `fwscanf`, `wscanf`, `swscanf`,
and `v*wscanf` ({need}`ICD_LIBC_0013`, {need}`SWR_LIBC_0014`).
```

```{comp} Ring 3 regex, glob, and wordexp components
:id: COMP_LIBC_0014
:status: active
:posix: XBD/regex.h (CX), XBD/glob.h (CX), XBD/wordexp.h (CX)
:refines: ARC_LIBC_0001, ICD_LIBC_0014
:satisfies: SWR_LIBC_0015

`libc/src/regex.c` implements POSIX BRE/ERE compilation, execution, `regerror`,
and `regfree`. `libc/src/glob.c` implements `glob` and `globfree` over
`opendir`, `readdir`, `fnmatch`, and `lstat`/`stat`. `libc/src/wordexp.c`
implements `wordexp` and `wordfree` over quoting, parameter and arithmetic
expansion, `popen`, `IFS` splitting, and `glob` ({need}`ICD_LIBC_0014`,
{need}`SWR_LIBC_0015`).
```

```{comp} Ring 3 i18n libc components and CLI utilities
:id: COMP_LIBC_0015
:status: active
:posix: XBD/langinfo.h, monetary.h, nl_types.h, iconv.h, libintl.h (BASE)
:refines: ARC_LIBC_0001, ICD_LIBC_0015
:satisfies: SWR_LIBC_0016

`libc/src/langinfo.c`, `monetary.c`, `iconv.c`, `nl_types.c`, and `libintl.c`
implement all 25 Base i18n interfaces in Ring 3. `bin/utils/locale.c`,
`localedef.c`, `iconv.c`, `gencat.c`, `msgfmt.c`, `gettext.c`, and `ngettext.c`
implement the seven POSIX.1-2024 i18n CLI utilities ({need}`ICD_LIBC_0015`,
{need}`SWR_LIBC_0016`).
```

```{comp} Ring 3 ELF64 ET_DYN dynamic loader
:id: COMP_LIBC_0016
:status: active
:posix: XBD/dlfcn.h (BASE)
:refines: ARC_LIBC_0001, ICD_LIBC_0016
:satisfies: SWR_LIBC_0017

`libc/src/dlfcn.c` implements `dlopen`, `dlsym`, `dlclose`, `dlerror`, and
`dladdr` in Ring 3. It maps `ET_DYN` shared objects via `mmap`, processes
`x86-64` relocations, and enforces W^X via `mprotect`. It also runs init/fini
hooks and keeps per-thread `dlerror` state in `FS.base` TLS
({need}`ICD_LIBC_0016`, {need}`SWR_LIBC_0017`).
```

```{comp} Ring 3 modular POSIX.1-2024 XCU shell
:id: COMP_LIBC_0017
:status: active
:posix: XCU/2 (BASE), XCU/dot, XCU/colon, XCU/alias, XCU/command, XCU/getopts
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0018

`bin/sh/lexer.c`, `parser.c`, `expand.c`, `exec.c`, `builtins.c`, and `main.c`
implement the Ring 3 command shell AST and compound commands. They also
implement deferred expansions, here-documents, `<glob.h>` globbing, `trap`, and
all standard POSIX.1-2024 built-in utilities ({need}`ARC_LIBC_0001`,
{need}`SWR_LIBC_0018`).
```

```{comp} Ring 3 grep, sed, find, xargs, expr, and printf utilities
:id: COMP_LIBC_0018
:status: active
:posix: XCU/grep, XCU/sed, XCU/find, XCU/xargs, XCU/expr, XCU/printf (BASE)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0019

`bin/utils/grep.c`, `sed.c`, `find.c`, `xargs.c`, `expr.c`, and `printf.c`
implement regex search, stream editing, and directory traversal in Ring 3. They
also implement NUL-safe argument batching, expression evaluation, and formatted
output ({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0019`).
```

```{comp} Ring 3 POSIX network socket dispatch, inet, if, and netdb library
:id: COMP_LIBC_0019
:status: active
:posix: XBD/netinet/in.h, netinet/tcp.h, arpa/inet.h, net/if.h, netdb.h (BASE)
:refines: ARC_LIBC_0001, ICD_LIBC_0017
:satisfies: SWR_LIBC_0020

`libc/src/socket.c` and `libc/src/fd.c` dispatch `AF_INET` and `AF_INET6`
sockets, `sendmsg`, `recvmsg`, `sockatmark`, and multi-server `ppoll`/`select`
over `/dev/net`. `libc/src/inet.c` and `libc/src/netdb.c` implement byte-order
helpers, `inet_*`, live `/dev/net` `if_*` queries, `getaddrinfo`, `getnameinfo`,
`gai_strerror`, and `get*ent` ({need}`ICD_LIBC_0017`, {need}`SWR_LIBC_0020`).
```

```{comp} Ring 3 sort, uniq, cut, tr, tail, tee, paste, join, and cksum tools
:id: COMP_LIBC_0020
:status: active
:posix: XCU/sort, XCU/uniq, XCU/cut, XCU/tr, XCU/tail, XCU/tee, XCU/paste
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0021

`bin/utils/sort.c`, `uniq.c`, `cut.c`, `tr.c`, `tail.c`, `tee.c`, `paste.c`,
`join.c`, and `cksum.c` implement Ring 3 text-pipeline tools. They supply
multi-key sorting, deduplication, field cutting, byte translation, tail/tee,
pasting, joining, and POSIX.1-2024 CRC-32 checksums ({need}`ARC_LIBC_0001`,
{need}`SWR_LIBC_0021`).
```

```{comp} Ring 3 diff, cmp, comm, and patch utilities
:id: COMP_LIBC_0021
:status: active
:posix: XCU/diff, XCU/cmp, XCU/comm, XCU/patch (BASE)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0022

`bin/utils/cmp.c`, `comm.c`, `diff.c`, and `patch.c` implement byte and sorted
file comparisons, LCS unified/context/normal/ed diffs, and reversible patch
application in Ring 3 ({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0022`).
```

```{comp} Ring 3 pax archive utility (ustar and cpio)
:id: COMP_LIBC_0022
:status: active
:posix: XCU/pax (BASE)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0023

`bin/utils/pax.c` implements POSIX.1-2024 `pax` list, extract, write, and `-rw`
copy-pass modes in Ring 3. It supports `<tar.h>` `ustar` and `<cpio.h>` ODC
archives with `-s` regex path substitution ({need}`ARC_LIBC_0001`,
{need}`SWR_LIBC_0023`).
```

```{comp} Ring 3 split, format, and inspection utilities
:id: COMP_LIBC_0023
:status: active
:posix: XCU/split, XCU/csplit, XCU/fold, XCU/expand, XCU/unexpand, XCU/strings
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0024

`bin/utils/split.c`, `csplit.c`, `fold.c`, `expand.c`, `unexpand.c`,
`strings.c`, `tsort.c`, `file.c`, and `pr.c` implement nine Ring 3 XCU tools.
They supply file splitting, column folding, tab conversion, string extraction,
topological sorting, file classification, and paginated printing
({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0024`).
```

```{comp} Ring 3 awk interpreter and ed line editor
:id: COMP_LIBC_0024
:status: active
:posix: XCU/awk, XCU/ed (BASE)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0025

`bin/awk/` (`awk.h`, `lex.c`, `parse.c`, `eval.c`, `main.c`) implements the Ring
3 `awk` language over `<regex.h>` and `<math.h>`. `bin/utils/ed.c` implements
the Ring 3 `ed` line editor with BRE addressing, substitution, global commands,
and undo ({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0025`).
```

```{comp} Ring 3 bc calculator and m4 macro processor
:id: COMP_LIBC_0025
:status: active
:posix: XCU/bc, XCU/m4 (BASE)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0026

`bin/bc/` (`bc.h`, `num.c`, `eval.c`, `main.c`) implements arbitrary-precision
decimal `bc` with `-l` math functions. `bin/utils/m4.c` implements the Ring 3
`m4` macro processor with pushback expansion, `eval`, and diversions
({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0026`).
```

```{comp} Ring 3 encoding, logging, terminal, and user utilities
:id: COMP_LIBC_0026
:status: active
:posix: XCU/uuencode, XCU/uudecode, XCU/logger, XCU/tput, XCU/tabs, XCU/mesg
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0027

`bin/utils/uuencode.c`, `uudecode.c`, `logger.c`, `tput.c`, `tabs.c`, `mesg.c`,
`write.c`, and `newgrp.c` implement eight Ring 3 XCU tools. They supply
encoding, syslog logging, terminal control, messaging, and group switching
({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0027`).
```

```{comp} Ring 3 at, batch, crontab, lp, and mailx utilities
:id: COMP_LIBC_0027
:status: active
:posix: XCU/at, XCU/batch, XCU/crontab, XCU/lp, XCU/mailx (BASE)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0028

`bin/utils/at.c`, `batch.c`, `crontab.c`, `lp.c`, and `mailx.c` implement
deferred job queues, cron schedule tables, print spooling, and local `mbox` mail
in Ring 3 ({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0028`).
```
