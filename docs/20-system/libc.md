# System and Software Requirements — LIBC

The `LIBC` domain includes the freestanding Ring 3 C17 runtime, public headers,
syscall wrappers, and POSIX.1-2024 Issue 8 Base rules.

```{sys} Give a freestanding C17 library for POSIX.1-2024
:id: SYS_LIBC_0001
:status: active

The system must give a freestanding Ring 3 C17 library and public header set.
These interfaces must operate for user executables and obey POSIX.1-2024 Issue 8
Base rules.
```

```{swr} Give freestanding crt0, C helpers, and syscall wrappers
:id: SWR_LIBC_0001
:status: active
:derives: SYS_LIBC_0001

The C library must give `crt0`, the signal trampoline, console output, string
and heap helpers, and syscall wrappers. All C library code must compile with
`-ffreestanding -Wall -Wextra -Werror` ({need}`SYS_LIBC_0001`).
```

```{swr} Keep Issue 8 headers isolated with clean namespaces
:id: SWR_LIBC_0002
:status: active
:posix: XBD/unistd.h (BASE), XBD/13-14, XSH/2.2
:derives: SYS_LIBC_0001
:traces: BDR_0003, ADR_0003

Each POSIX.1-2024 header must compile alone with `-std=c17 -Wall -Wextra -Werror
-pedantic`, `-D_POSIX_C_SOURCE=202405L`, and `-D_XOPEN_SOURCE=800` ([XBD
13][xbd-13], [XBD 14][xbd-14], [XSH 2.2][xsh-02]). `<unistd.h>`
([unistd.h][hdr-unistd]) must set `_POSIX_VERSION` and `_POSIX2_VERSION` to
`202405L` and must not include `<sys/microkernel.h>`. It must keep internal
names in `__` or `_[A-Z]` and put option groups behind `_POSIX_*` macros
({need}`SYS_LIBC_0001`).
```

```{swr} Give thread-local errno isolated from raw syscalls
:id: SWR_LIBC_0003
:status: active
:posix: XBD/errno.h (CX), XSH/2.3
:derives: SYS_LIBC_0001
:traces: BDR_0003, ADR_0003

`<errno.h>` ([errno.h][hdr-errno]) must give thread-local `errno` to obey [XSH
2.3][xsh-02]. Libc wrappers must isolate raw syscall error codes from `errno`
and set `errno` only on an error. Wrappers must not set `errno` to 0 on success
and must obey the mandatory error sequence ({need}`SYS_LIBC_0001`).
```

```{swr} Give mandatory POSIX.1-2024 thread cancellation points
:id: SWR_LIBC_0004
:status: active
:posix: XSH/2.9.5 (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, ADR_0003, ADR_0013

Libc wrappers for mandatory cancellation points (`read`, `write`, `open`,
`close`, `nanosleep`, `clock_nanosleep`, `ppoll`, `pthread_cond_wait`,
`pthread_cond_clockwait`, `sem_wait`, `waitpid`, and the full [XSH 2.9][xsh-02]
set) must examine deferred cancellation at safe points when the thread does not
lock a mutex ({need}`SYS_LIBC_0001`).
```

```{swr} Give new POSIX.1-2024 Base functions and C17/C11 headers
:id: SWR_LIBC_0005
:status: active
:posix: XSH/getentropy (BASE), XSH/strlcpy (CX), XBD/stdatomic.h (CX)
:derives: SYS_LIBC_0001
:traces: BDR_0003, ADR_0003, ADR_0014

With `-D_POSIX_C_SOURCE=202405L`, `libc` must give new Issue 8 Base functions
`strlcpy`, `strlcat`, `reallocarray`, `getentropy` ([getentropy][fn-getent]),
and `secure_getenv`. It must also give `<stdatomic.h>`, `<stdalign.h>`, and
`<uchar.h>` ({need}`SYS_LIBC_0001`).
```

```{swr} Give Ring 3 POSIX.1-2024 XCU shell utility
:id: SWR_LIBC_0006
:status: active
:posix: XCU/2 (BASE), XCU/read (BASE), XCU/set (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, ADR_0003, ADR_0021

A Ring 3 command shell (`sh`) and primary utilities must do pipelines and
redirections with `fork`, `execvp`, `pipe2`, and `dup2`. The shell must include
Issue 8 `$'...'` syntax, `set -o pipefail`, and `read -d`
({need}`SYS_LIBC_0001`).
```

```{swr} Give POSIX.1-2024 buffered stdio, env, execvp, and primary headers
:id: SWR_LIBC_0007
:status: active
:posix: XBD/ctype.h (CX), XSH/fopen (CX), XSH/setenv (BASE), XSH/exec (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, ADR_0003, ADR_0019

`libc` must give `<ctype.h>`, `<assert.h>`, `<inttypes.h>`, `<strings.h>`, and
`<limits.h>` with POSIX limits. It must also give buffered `<stdio.h>` (`fopen`,
`fdopen`, `fclose`, `fread`, `fwrite`, `fgets`, `fseek`, `fileno`), `setenv`,
`unsetenv`, `execv`, `execvp`, `strdup`, `strstr`, and `strerror`
({need}`SYS_LIBC_0001`).
```

```{swr} Give more POSIX.1-2024 Base headers and standard C utilities
:id: SWR_LIBC_0008
:status: active
:posix: XBD/sys/utsname.h (BASE), XBD/endian.h (BASE), XBD/sched.h (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, ADR_0003

`libc` must record compiler freestanding headers in Gate 2 and give
`<sys/utsname.h>`, `<endian.h>`, `<sched.h>`, `<setjmp.h>`, `<sys/times.h>`, and
`<fnmatch.h>`. It must also give `atexit`, `qsort`, `bsearch`, `strtol`,
`strtoul`, `getopt`, `alarm`, `rename`, and `realpath` ({need}`SYS_LIBC_0001`).
```

```{swr} Give POSIX.1-2024 <math.h> and <fenv.h> with x86-64-v3 FMA
:id: SWR_LIBC_0009
:status: active
:posix: XBD/math.h (CX), XBD/fenv.h (CX)
:derives: SYS_LIBC_0001
:traces: BDR_0003, ADR_0003, ADR_0026

`libc` must give `<fenv.h>` (`FE_*` modes and IEEE-754 error flags through
`MXCSR`) and `<math.h>` (IEEE-754 class, `round`, `sqrt`, `fma`, exponential,
logarithmic, `pow`, and trigonometric functions) compiled for `x86-64-v3` with
hardware `FMA` ({need}`SYS_LIBC_0001`).
```

```{swr} Give full Base <stdio.h>, <stdlib.h>, <string.h>, and <time.h>
:id: SWR_LIBC_0010
:status: active
:posix: XBD/stdio.h (CX), XBD/stdlib.h (CX), XBD/string.h (CX), XBD/time.h (CX)
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0032

`libc` must give Base `<stdio.h>` input scan, line input, descriptor and string
format helpers, memory streams, `freopen`, and `fgetpos`/`fsetpos` in Ring 3. It
must give `<stdlib.h>` `strtod`/`strtof`/`strtold`/`atof`, `aligned_alloc`,
`quick_exit`/`at_quick_exit`, `div`/`ldiv`/`lldiv`, `rand`/`srand`, `qsort_r`,
and `getsubopt`. It must give `stpcpy`, `stpncpy`, `memmem`, `strerror_r`, and
Base `<time.h>` calendar, `strftime`, `clock`, and `timespec_get` interfaces
({need}`SYS_LIBC_0001`).
```

```{swr} Give ISO C17 and POSIX.1-2024 <complex.h> and <tgmath.h>
:id: SWR_LIBC_0011
:status: active
:posix: XBD/complex.h (CX), XBD/tgmath.h (CX)
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0033

`libc` must give `<complex.h>` (`CMPLX*`, `cabs*`, `carg*`, `cimag*`, `creal*`,
`conj*`, `cproj*`, `csqrt*`, `cexp*`, `clog*`, `cpow*`, and complex
trigonometric and hyperbolic functions). It must also give `<tgmath.h>`
type-generic macros for `<math.h>` and `<complex.h>` in Ring 3
({need}`SYS_LIBC_0001`).
```

```{swr} Give Base futimens, utimensat, lgamma*, tzset, and tzname
:id: SWR_LIBC_0012
:status: active
:posix: XBD/sys_stat.h (CX), XBD/math.h (CX), XBD/time.h (CX)
:derives: SYS_LIBC_0001, SYS_IPC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0034

`libc` and `vfsserv` must give async-signal-safe `futimens` and `utimensat` with
`UTIME_NOW` and `UTIME_OMIT` in Ring 3. `libc` must give `lgamma`, `lgammaf`,
and `lgammal` in `<math.h>` and `<tgmath.h>`, and `tzset` and `tzname` in
`<time.h>`. It must not show XSI symbols with `_POSIX_C_SOURCE=202405L`
({need}`SYS_LIBC_0001`, {need}`SYS_IPC_0001`).
```

```{swr} Give <locale.h>, *_l helpers, multibyte, and primary wchar/wctype
:id: SWR_LIBC_0013
:status: active
:posix: XBD/locale.h (CX), XBD/wchar.h (CX), XBD/wctype.h (CX)
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0035

`libc` must give `<locale.h>` (`C` and `C.UTF-8` locales, `setlocale`,
`localeconv`, `newlocale`, `duplocale`, `freelocale`, `uselocale`, and
`getlocalename_l`) and `*_l` helpers in `<ctype.h>`, `<strings.h>`,
`<string.h>`, and `<time.h>`. It must give `<stdlib.h>` multibyte conversions,
`<wctype.h>`, primary `<wchar.h>` (`btowc`, `wctob`, `mbr*`, `mbs*`, `wcrtomb`,
`wmem*`, `wcs*`, `wcsto*`, `wcsftime`), and `<inttypes.h>`
`wcstoimax`/`wcstoumax` in Ring 3 ({need}`SYS_LIBC_0001`).
```

```{swr} Give wide-oriented FILE I/O, *wprintf, *wscanf, and open_wmemstream
:id: SWR_LIBC_0014
:status: active
:posix: XBD/wchar.h (CX), XBD/stdio.h (CX)
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0036

`libc` must extend Ring 3 `FILE` streams with wide orientation (`fwide`), wide
character I/O (`fgetwc`, `fgetws`, `fputwc`, `fputws`, `getwc`, `getwchar`,
`putwc`, `putwchar`, `ungetwc`), wide formatted I/O (`fwprintf`, `wprintf`,
`swprintf`, `vfwprintf`, `vwprintf`, `vswprintf`, `fwscanf`, `wscanf`,
`swscanf`, `vfwscanf`, `vwscanf`, `vswscanf`), and `open_wmemstream`
({need}`SYS_LIBC_0001`).
```

```{swr} Give <regex.h>, <glob.h>, <wordexp.h>, <tar.h>, and <cpio.h>
:id: SWR_LIBC_0015
:status: active
:posix: XBD/regex.h, XBD/glob.h, XBD/wordexp.h, XBD/tar.h, XBD/cpio.h (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0037

`libc` must give POSIX.1-2024 BRE and ERE pattern lookup (`regcomp`, `regexec`,
`regerror`, `regfree`), path lookup (`glob`, `globfree`), and shell word
expansion (`wordexp`, `wordfree`) in Ring 3. It must also give `<tar.h>` and
`<cpio.h>` archive format headers ({need}`SYS_LIBC_0001`).
```

```{swr} Give i18n headers, message catalogs, and i18n CLI utilities
:id: SWR_LIBC_0016
:status: active
:posix: XBD/langinfo.h, monetary.h, nl_types.h, iconv.h, libintl.h (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0038

`libc` must give `<langinfo.h>`, `<monetary.h>`, `<nl_types.h>`, `<iconv.h>`,
and `<libintl.h>` in Ring 3. User space must give `locale`, `localedef`,
`iconv`, `gencat`, `msgfmt`, `gettext`, and `ngettext` ({need}`SYS_LIBC_0001`).
```

```{swr} Give Ring 3 ELF64 ET_DYN dynamic load and <dlfcn.h>
:id: SWR_LIBC_0017
:status: active
:posix: XBD/dlfcn.h (BASE)
:derives: SYS_LIBC_0001, SYS_PROC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0039

`libc` must give `<dlfcn.h>` (`dlopen`, `dlsym`, `dlclose`, `dlerror`, `dladdr`)
in Ring 3. The loader must map `ET_DYN` objects with W^X `mmap` and `mprotect`,
do `x86-64` relocations, and do constructors and destructors
({need}`SYS_LIBC_0001`, {need}`SYS_PROC_0001`).
```

```{swr} Give POSIX.1-2024 XCU shell compound grammar and built-ins
:id: SWR_LIBC_0018
:status: active
:posix: XCU/2 (BASE), XCU/dot, XCU/colon, XCU/alias, XCU/command, XCU/getopts
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0040

`/boot/sh` must give `if`, `for`, `while`, `until`, `case`, subshells, `{ ... }`
groups, functions, `$(...)`, `$((...))`, parameter expansions, here-documents,
`<glob.h>` path expansion, and `trap`. It must also give `.`, `:`, `alias`,
`unalias`, `command`, `eval`, `exec`, `getopts`, `hash`, `return`, `shift`, and
`times` ({need}`SYS_LIBC_0001`).
```

```{swr} Give POSIX.1-2024 XCU grep, sed, find, xargs, expr, and printf
:id: SWR_LIBC_0019
:status: active
:posix: XCU/grep, XCU/sed, XCU/find, XCU/xargs, XCU/expr, XCU/printf (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0041

User space must give isolated Ring 3 utilities `grep`, `sed`, `find`, `xargs`,
`expr`, and `printf` with `<regex.h>` and `<fnmatch.h>` ({need}`SYS_LIBC_0001`).
```

```{swr} Give POSIX.1-2024 network Base headers, AF_INET/6 sockets, and netdb
:id: SWR_LIBC_0020
:status: active
:posix: XBD/netinet/in.h, netinet/tcp.h, arpa/inet.h, net/if.h, netdb.h (BASE)
:derives: SYS_LIBC_0001, SYS_IPC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0043

`libc` must connect `AF_INET` and `AF_INET6` sockets through `/dev/net` and give
`sendmsg`, `recvmsg`, and `sockatmark`. It must also give `<netinet/in.h>`,
`<netinet/tcp.h>`, `<arpa/inet.h>`, `<net/if.h>`, and `<netdb.h>`
(`hton*`/`ntoh*`, `inet_*`, `if_*`, `getaddrinfo`, `getnameinfo`,
`gai_strerror`, `get*ent`) ({need}`SYS_LIBC_0001`, {need}`SYS_IPC_0001`).
```

```{swr} Give POSIX.1-2024 XCU sort, field, and checksum text utilities
:id: SWR_LIBC_0021
:status: active
:posix: XCU/sort, XCU/uniq, XCU/cut, XCU/tr, XCU/tail, XCU/tee, XCU/paste
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0044

User space must give isolated Ring 3 utilities `sort`, `uniq`, `cut`, `tr`,
`tail`, `tee`, `paste`, `join`, and `cksum` with `libc`, `vfsserv`, and
`pipeserv` ({need}`SYS_LIBC_0001`).
```

```{swr} Give POSIX.1-2024 XCU diff, cmp, comm, and patch utilities
:id: SWR_LIBC_0022
:status: active
:posix: XCU/diff, XCU/cmp, XCU/comm, XCU/patch (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0045

User space must give isolated Ring 3 utilities `diff`, `cmp`, `comm`, and
`patch` with `libc` and `vfsserv` ({need}`SYS_LIBC_0001`).
```

```{swr} Give POSIX.1-2024 XCU pax archive utility
:id: SWR_LIBC_0023
:status: active
:posix: XCU/pax (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0046

User space must give the Ring 3 `pax` archive utility with `list`, `read`,
`write`, and `copy-pass` modes for `<tar.h>` `ustar` and `<cpio.h>` formats
({need}`SYS_LIBC_0001`).
```

```{swr} Give POSIX.1-2024 XCU split, format, and check utilities
:id: SWR_LIBC_0024
:status: active
:posix: XCU/split, XCU/csplit, XCU/fold, XCU/expand, XCU/unexpand, XCU/strings
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0047

User space must give isolated Ring 3 utilities `split`, `csplit`, `fold`,
`expand`, `unexpand`, `strings`, `tsort`, `file`, and `pr` with `libc`,
`vfsserv`, and `pipeserv` ({need}`SYS_LIBC_0001`).
```

```{swr} Give POSIX.1-2024 XCU awk and ed utilities
:id: SWR_LIBC_0025
:status: active
:posix: XCU/awk, XCU/ed (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0048

User space must give the Ring 3 `awk` pattern language and `ed` line editor with
`<regex.h>`, `<math.h>`, `vfsserv`, and `pipeserv` ({need}`SYS_LIBC_0001`).
```

```{swr} Give POSIX.1-2024 XCU bc and m4 language utilities
:id: SWR_LIBC_0026
:status: active
:posix: XCU/bc, XCU/m4 (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0049

User space must give the Ring 3 arbitrary-precision calculator `bc` (`-l`) and
macro processor `m4` (`-D`, `-U`, `-s`) with `libc`, `vfsserv`, and `pipeserv`
({need}`SYS_LIBC_0001`).
```

```{swr} Give POSIX.1-2024 XCU format, log, terminal, and user utilities
:id: SWR_LIBC_0027
:status: active
:posix: XCU/uuencode, XCU/uudecode, XCU/logger, XCU/tput, XCU/tabs, XCU/mesg
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0050

User space must give isolated Ring 3 utilities `uuencode`, `uudecode`, `logger`,
`tput`, `tabs`, `mesg`, `write`, and `newgrp` with `libc`, `vfsserv`, and
`ttyserv` ({need}`SYS_LIBC_0001`).
```

```{swr} Give POSIX.1-2024 XCU schedule, print, and mail utilities
:id: SWR_LIBC_0028
:status: active
:posix: XCU/at, XCU/batch, XCU/crontab, XCU/lp, XCU/mailx (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0051

User space must give isolated Ring 3 utilities `at`, `batch`, `crontab`, `lp`,
and `mailx` with `libc` and `vfsserv` ({need}`SYS_LIBC_0001`).
```

```{swr} Give XSI syslog, crypt, time, uio, and OpenSSH libc APIs
:id: SWR_LIBC_0029
:status: active
:posix: XBD/syslog.h, XBD/sys/time.h, XBD/sys/uio.h, XSH/crypt (XSI)
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0053

`libc` must give `<syslog.h>`, `<sys/time.h>`, `<sys/uio.h>`, `crypt`,
`getrusage`, `setpwent`/`getpwent`/`endpwent`, `setgrent`/`getgrent`/`endgrent`,
and `setreuid`/`setregid`/`setresuid`/`setresgid`. It must also give OpenSSH
compatibility helpers (`vsyslog`, `gettimeofday`, `futimes`, `setgroups`,
`initgroups`, `getgrouplist`, `daemon`, `chroot`) behind feature macros
({need}`SYS_LIBC_0001`).
```

```{swr} Give compiled Ring 3 sysroot zlib (libz.a and zlib.h)
:id: SWR_LIBC_0030
:status: active
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0055

The CMake build must download and compile `zlib` for Ring 3 into the sysroot
(`libz.a`, `<zlib.h>`, and `<zconf.h>`) and must not keep upstream source in
git. It must give one-shot compression, stream `deflate` and `inflate`, gzip
format, `gz*` file I/O, and `crc32`/`adler32` checksums ({need}`SYS_LIBC_0001`).
```

```{swr} Give compiled Ring 3 sysroot OpenSSL (libcrypto and libssl)
:id: SWR_LIBC_0031
:status: active
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0056

The CMake build must download and compile OpenSSL for Ring 3 into the sysroot
(`libcrypto.a`, `libssl.a`, and `<openssl/*.h>`) and must not keep upstream
source in git. It must give `/dev/urandom` seed input, SHA-256, AES-GCM, RSA,
ECDSA, Ed25519, X.509 certificates, and TLS 1.3 ({need}`SYS_LIBC_0001`).
```

```{swr} Give the freestanding Ring 3 dynamic linker /boot/ld.so
:id: SWR_LIBC_0032
:status: active
:derives: SYS_PROC_0001
:traces: ADR_0057, BDR_0003, BDR_0004

The freestanding `/boot/ld.so` must relocate itself and load `DT_NEEDED`
libraries through `/dev/vfs`. It must examine `LD_LIBRARY_PATH` before the
`/boot`, `/lib`, and `/usr/lib` defaults. It must do `R_X86_64_*` relocations,
obey W^X for each `PT_LOAD`, do `DT_INIT` and `DT_INIT_ARRAY`, and go to
`AT_ENTRY` with the initial stack ({need}`SYS_PROC_0001`).
```

```{swr} Give the shared libc.so and send dlfcn calls to ld.so
:id: SWR_LIBC_0033
:status: active
:derives: SYS_LIBC_0001
:traces: ADR_0059, BDR_0003, BDR_0004

The build must make a `-fPIC` shared `libc.so` with soname `libc.so` in the Ring
3 sysroot and `boot.tar` with static `libc.a`. Dynamic executables must link as
`ET_DYN` PIE images with `PT_INTERP=/boot/ld.so`. The linker script must keep
the ELF header and program header table in the first `R-X` segment. When `libc`
operates as `libc.so`, `dlopen`, `dlsym`, `dlclose`, and `dladdr` must send
calls to `/boot/ld.so`. The dynamic linker must obey W^X and give error status,
and `dlerror` must stay thread-local ({need}`SYS_LIBC_0001`).
```

```{swr} Give the shared sysroot zlib (libz.so.1)
:id: SWR_LIBC_0034
:status: active
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0055, ADR_0059

The CMake build must make a shared `libz.so.1` with soname `libz.so.1` with
static `libz.a`. It must install `libz.so.1` and a `libz.so` symbolic link in
the Ring 3 sysroot and include `libz.so.1` in `boot.tar`. Ring 3 executables
must link against `libz.so.1` as an `ET_DYN` `DT_NEEDED` dependency of the
process ({need}`SYS_LIBC_0001`).
```

```{swr} Supply the shared sysroot OpenSSL (libcrypto.so.3 and libssl.so.3)
:id: SWR_LIBC_0035
:status: active
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0056, ADR_0059

The CMake build must make shared `libcrypto.so.3` and `libssl.so.3` with sonames
`libcrypto.so.3` and `libssl.so.3` with static `libcrypto.a` and `libssl.a`. It
must not keep upstream source in git. It must install both shared libraries and
their `libcrypto.so` and `libssl.so` symbolic links in the Ring 3 sysroot and
include them in `boot.tar`. Ring 3 executables must link against them as
`ET_DYN` `DT_NEEDED` dependencies of the process ({need}`SYS_LIBC_0001`).
```

```{swr} Give the ldd trace mode of /boot/ld.so
:id: SWR_LIBC_0036
:status: active
:derives: SYS_LIBC_0001
:traces: WDR_0012, BDR_0003, BDR_0004, ADR_0059

`/boot/ld.so` must operate in ldd trace mode when `argv[0]` ends in `ldd` or the
environment sets `LD_TRACE_LOADED_OBJECTS=1`. It must find `DT_NEEDED` libraries
across `/etc/ld.so.conf` and the `/boot`, `/lib`, and `/usr/lib` defaults. It
must map each library, write `libname => /path (0xaddr)`, and exit without
giving control to `AT_ENTRY` ({need}`SYS_LIBC_0001`).
```

```{swr} Give native /usr/sbin/ldconfig for DT_SONAME symlinks
:id: SWR_LIBC_0037
:status: active
:derives: SYS_LIBC_0001
:traces: WDR_0012, BDR_0003, BDR_0004, ADR_0059

The build must give native `/usr/sbin/ldconfig` linked dynamically to `libc.so`.
It must read `/etc/ld.so.conf` and the `/boot`, `/lib`, and `/usr/lib` defaults,
read `DT_SONAME` from each shared library, and make missing `<dir>/<soname>`
symbolic links. It must exit with status 0 ({need}`SYS_LIBC_0001`).
```

```{swr} Give /usr/bin/getent, /etc/nsswitch.conf, and C.UTF-8 locale
:id: SWR_LIBC_0038
:status: active
:derives: SYS_LIBC_0001
:traces: WDR_0012, BDR_0003, BDR_0004

The build must give native `/usr/bin/getent` linked dynamically to `libc.so`. It
must look up and enumerate `passwd`, `group`, `hosts`, `services`, and
`protocols` entries through `libc` NSS functions. `vfsserv` must install
`/etc/nsswitch.conf` with small `files` defaults. `libc` (`libc.so` and
`libc.a`) must operate with `C` and `C.UTF-8` in `setlocale` and `nl_langinfo`
without external `LC_*` files in `boot.tar` ({need}`SYS_LIBC_0001`).
```

```{swr} Give IANA tzcode (zic, zdump, tzselect) for Ring 3
:id: SWR_LIBC_0039
:status: active
:derives: SYS_LIBC_0001
:traces: WDR_0012, BDR_0003, BDR_0004, ADR_0054

The CMake build must download and compile public-domain IANA `tzcode` for Ring
3. It must link `zic` and `zdump` dynamically to `libc.so`. It must install
`zic`, `zdump`, `tzselect`, and `tzdata` files into the Ring 3 sysroot and
`boot.tar`. The repository must not keep upstream source in git
({need}`SYS_LIBC_0001`).
```

```{swr} Give libarchive bsdtar (tar) for Ring 3
:id: SWR_LIBC_0040
:status: active
:derives: SYS_LIBC_0001
:traces: WDR_0012, BDR_0003, BDR_0004, ADR_0054

The CMake build must download and compile `libarchive` `bsdtar` for Ring 3. It
must link `tar` dynamically to `libc.so` and `libz.so.1`. It must install
`/usr/bin/tar` into the Ring 3 sysroot and `boot.tar`. The repository must not
keep upstream source in git ({need}`SYS_LIBC_0001`).
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
