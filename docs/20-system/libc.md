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
:status: active
:posix: XCU/2 (BASE), XCU/read (BASE), XCU/set (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, ADR_0003, ADR_0021

A Ring 3 command shell (`sh`) and minimal core utilities must execute pipelines
and redirections over `fork`/`execvp`/`pipe2`/`dup2`. The shell must aim to
follow Issue 8 `$'...'` quoting, `set -o pipefail`, and `read -d`
({need}`SYS_LIBC_0001`).
```

```{swr} Supply POSIX.1-2024 buffered stdio, env, execvp, and core headers
:id: SWR_LIBC_0007
:status: active
:posix: XBD/ctype.h (CX), XSH/fopen (CX), XSH/setenv (BASE), XSH/exec (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, ADR_0003, ADR_0019

`libc` must supply `<ctype.h>`, `<assert.h>`, `<inttypes.h>`, `<strings.h>`, and
`<limits.h>` with POSIX limits. It must also supply buffered `<stdio.h>`
(`fopen`, `fdopen`, `fclose`, `fread`, `fwrite`, `fgets`, `fseek`, `fileno`),
`setenv`, `unsetenv`, `execv`, `execvp`, `strdup`, `strstr`, and `strerror`
({need}`SYS_LIBC_0001`).
```

```{swr} Supply additional POSIX.1-2024 Base headers and standard C utilities
:id: SWR_LIBC_0008
:status: active
:posix: XBD/sys/utsname.h (BASE), XBD/endian.h (BASE), XBD/sched.h (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, ADR_0003

`libc` must register compiler freestanding headers in Gate 2 and supply
`<sys/utsname.h>`, `<endian.h>`, `<sched.h>`, `<setjmp.h>`, `<sys/times.h>`, and
`<fnmatch.h>`. It must also add `atexit`, `qsort`, `bsearch`, `strtol`,
`strtoul`, `getopt`, `alarm`, `rename`, and `realpath` ({need}`SYS_LIBC_0001`).
```

```{swr} Supply POSIX.1-2024 <math.h> and <fenv.h> with x86-64-v3 FMA support
:id: SWR_LIBC_0009
:status: active
:posix: XBD/math.h (CX), XBD/fenv.h (CX)
:derives: SYS_LIBC_0001
:traces: BDR_0003, ADR_0003, ADR_0026

`libc` must supply `<fenv.h>` (rounding modes and exception flags via `MXCSR`)
and `<math.h>` (IEEE-754 classification, rounding, `sqrt`, `fma`, exponential,
logarithmic, power, and trigonometric functions) compiled for `x86-64-v3` with
hardware `FMA` ({need}`SYS_LIBC_0001`).
```

```{swr} Complete Base <stdio.h>, <stdlib.h>, <string.h>, and <time.h>
:id: SWR_LIBC_0010
:status: active
:posix: XBD/stdio.h (CX), XBD/stdlib.h (CX), XBD/string.h (CX), XBD/time.h (CX)
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0032

`libc` must supply Base `<stdio.h>` scanning, line input, descriptor and string
format helpers, memory streams, `freopen`, and `fgetpos`/`fsetpos` in Ring 3. It
must supply `<stdlib.h>` `strtod`/`strtof`/`strtold`/`atof`, `aligned_alloc`,
`quick_exit`/`at_quick_exit`, `div`/`ldiv`/`lldiv`, `rand`/`srand`, `qsort_r`,
and `getsubopt`. It must supply `stpcpy`, `stpncpy`, `memmem`, `strerror_r`, and
Base `<time.h>` calendar, `strftime`, `clock`, and `timespec_get` interfaces
({need}`SYS_LIBC_0001`).
```

```{swr} Supply ISO C17 and POSIX.1-2024 <complex.h> and <tgmath.h>
:id: SWR_LIBC_0011
:status: active
:posix: XBD/complex.h (CX), XBD/tgmath.h (CX)
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0033

`libc` must supply `<complex.h>` (`CMPLX*`, `cabs*`, `carg*`, `cimag*`,
`creal*`, `conj*`, `cproj*`, `csqrt*`, `cexp*`, `clog*`, `cpow*`, and complex
trigonometric and hyperbolic functions). It must also supply `<tgmath.h>`
type-generic macros over `<math.h>` and `<complex.h>` in Ring 3
({need}`SYS_LIBC_0001`).
```

```{swr} Supply Base futimens, utimensat, lgamma*, tzset, and tzname
:id: SWR_LIBC_0012
:status: active
:posix: XBD/sys_stat.h (CX), XBD/math.h (CX), XBD/time.h (CX)
:derives: SYS_LIBC_0001, SYS_IPC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0034

`libc` and `vfsserv` must supply async-signal-safe `futimens` and `utimensat`
with `UTIME_NOW` and `UTIME_OMIT` in Ring 3. `libc` must supply `lgamma`,
`lgammaf`, and `lgammal` in `<math.h>` and `<tgmath.h>`, plus `tzset` and
`tzname` in `<time.h>`, with no XSI symbol leakage under
`_POSIX_C_SOURCE=202405L` ({need}`SYS_LIBC_0001`, {need}`SYS_IPC_0001`).
```

```{swr} Supply <locale.h>, *_l helpers, multibyte, and core wchar/wctype
:id: SWR_LIBC_0013
:status: active
:posix: XBD/locale.h (CX), XBD/wchar.h (CX), XBD/wctype.h (CX)
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0035

`libc` must supply `<locale.h>` (`C` and `C.UTF-8` locales, `setlocale`,
`localeconv`, `newlocale`, `duplocale`, `freelocale`, `uselocale`, and
`getlocalename_l`) and `*_l` helpers in `<ctype.h>`, `<strings.h>`,
`<string.h>`, and `<time.h>`. It must supply `<stdlib.h>` multibyte conversions,
`<wctype.h>`, core `<wchar.h>` (`btowc`, `wctob`, `mbr*`, `mbs*`, `wcrtomb`,
`wmem*`, `wcs*`, `wcsto*`, `wcsftime`), and `<inttypes.h>`
`wcstoimax`/`wcstoumax` in Ring 3 ({need}`SYS_LIBC_0001`).
```

```{swr} Supply wide-oriented FILE I/O, *wprintf, *wscanf, and open_wmemstream
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

```{swr} Supply <regex.h>, <glob.h>, <wordexp.h>, <tar.h>, and <cpio.h>
:id: SWR_LIBC_0015
:status: active
:posix: XBD/regex.h, XBD/glob.h, XBD/wordexp.h, XBD/tar.h, XBD/cpio.h (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0037

`libc` must supply POSIX.1-2024 BRE and ERE matching (`regcomp`, `regexec`,
`regerror`, `regfree`), pathname matching (`glob`, `globfree`), and shell word
expansion (`wordexp`, `wordfree`) in Ring 3. It must also supply `<tar.h>` and
`<cpio.h>` archive format headers ({need}`SYS_LIBC_0001`).
```

```{swr} Supply i18n headers, message catalogs, and i18n CLI utilities
:id: SWR_LIBC_0016
:status: active
:posix: XBD/langinfo.h, monetary.h, nl_types.h, iconv.h, libintl.h (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0038

`libc` must supply `<langinfo.h>`, `<monetary.h>`, `<nl_types.h>`, `<iconv.h>`,
and `<libintl.h>` in Ring 3. User space must supply `locale`, `localedef`,
`iconv`, `gencat`, `msgfmt`, `gettext`, and `ngettext` ({need}`SYS_LIBC_0001`).
```

```{swr} Supply Ring 3 ELF64 ET_DYN dynamic loading and <dlfcn.h>
:id: SWR_LIBC_0017
:status: active
:posix: XBD/dlfcn.h (BASE)
:derives: SYS_LIBC_0001, SYS_PROC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0039

`libc` must supply `<dlfcn.h>` (`dlopen`, `dlsym`, `dlclose`, `dlerror`,
`dladdr`) in Ring 3. The loader must map `ET_DYN` objects with W^X
`mmap`/`mprotect`, process `x86-64` relocations, and run constructors and
destructors ({need}`SYS_LIBC_0001`, {need}`SYS_PROC_0001`).
```

```{swr} Supply POSIX.1-2024 XCU shell compound grammar and built-ins
:id: SWR_LIBC_0018
:status: active
:posix: XCU/2 (BASE), XCU/dot, XCU/colon, XCU/alias, XCU/command, XCU/getopts
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0040

`/boot/sh` must support `if`, `for`, `while`, `until`, `case`, subshells, brace
groups, functions, `$(...)`, `$((...))`, parameter expansions, here-documents,
`<glob.h>` globbing, and `trap`. It must also supply `.`, `:`, `alias`,
`unalias`, `command`, `eval`, `exec`, `getopts`, `hash`, `return`, `shift`, and
`times` ({need}`SYS_LIBC_0001`).
```

```{swr} Supply POSIX.1-2024 XCU grep, sed, find, xargs, expr, and printf
:id: SWR_LIBC_0019
:status: active
:posix: XCU/grep, XCU/sed, XCU/find, XCU/xargs, XCU/expr, XCU/printf (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0041

User space must supply standalone Ring 3 utilities `grep`, `sed`, `find`,
`xargs`, `expr`, and `printf` over `<regex.h>` and `<fnmatch.h>`
({need}`SYS_LIBC_0001`).
```

```{swr} Supply POSIX.1-2024 network Base headers, AF_INET/6 sockets, and netdb
:id: SWR_LIBC_0020
:status: active
:posix: XBD/netinet/in.h, netinet/tcp.h, arpa/inet.h, net/if.h, netdb.h (BASE)
:derives: SYS_LIBC_0001, SYS_IPC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0043

`libc` must wire `AF_INET` and `AF_INET6` sockets over `/dev/net` and supply
`sendmsg`, `recvmsg`, and `sockatmark`. It must also supply `<netinet/in.h>`,
`<netinet/tcp.h>`, `<arpa/inet.h>`, `<net/if.h>`, and `<netdb.h>`
(`hton*`/`ntoh*`, `inet_*`, `if_*`, `getaddrinfo`, `getnameinfo`,
`gai_strerror`, `get*ent`) ({need}`SYS_LIBC_0001`, {need}`SYS_IPC_0001`).
```

```{swr} Supply POSIX.1-2024 XCU sort, field, and checksum text utilities
:id: SWR_LIBC_0021
:status: active
:posix: XCU/sort, XCU/uniq, XCU/cut, XCU/tr, XCU/tail, XCU/tee, XCU/paste
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0044

User space must supply standalone Ring 3 utilities `sort`, `uniq`, `cut`, `tr`,
`tail`, `tee`, `paste`, `join`, and `cksum` over `libc`, `vfsserv`, and
`pipeserv` ({need}`SYS_LIBC_0001`).
```

```{swr} Supply POSIX.1-2024 XCU diff, cmp, comm, and patch utilities
:id: SWR_LIBC_0022
:status: active
:posix: XCU/diff, XCU/cmp, XCU/comm, XCU/patch (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0045

User space must supply standalone Ring 3 utilities `diff`, `cmp`, `comm`, and
`patch` over `libc` and `vfsserv` ({need}`SYS_LIBC_0001`).
```

```{swr} Supply POSIX.1-2024 XCU pax archive utility
:id: SWR_LIBC_0023
:status: active
:posix: XCU/pax (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0046

User space must supply the Ring 3 `pax` archive utility supporting list, read,
write, and copy-pass modes over `<tar.h>` `ustar` and `<cpio.h>` formats
({need}`SYS_LIBC_0001`).
```

```{swr} Supply POSIX.1-2024 XCU split, format, and inspection utilities
:id: SWR_LIBC_0024
:status: active
:posix: XCU/split, XCU/csplit, XCU/fold, XCU/expand, XCU/unexpand, XCU/strings
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0047

User space must supply standalone Ring 3 utilities `split`, `csplit`, `fold`,
`expand`, `unexpand`, `strings`, `tsort`, `file`, and `pr` over `libc`,
`vfsserv`, and `pipeserv` ({need}`SYS_LIBC_0001`).
```

```{swr} Supply POSIX.1-2024 XCU awk and ed utilities
:id: SWR_LIBC_0025
:status: active
:posix: XCU/awk, XCU/ed (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0048

User space must supply the Ring 3 `awk` pattern-scanning language and `ed` line
editor over `<regex.h>`, `<math.h>`, `vfsserv`, and `pipeserv`
({need}`SYS_LIBC_0001`).
```

```{swr} Supply POSIX.1-2024 XCU bc and m4 language utilities
:id: SWR_LIBC_0026
:status: active
:posix: XCU/bc, XCU/m4 (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0049

User space must supply the Ring 3 arbitrary-precision calculator `bc` (`-l`) and
macro processor `m4` (`-D`, `-U`, `-s`) over `libc`, `vfsserv`, and `pipeserv`
({need}`SYS_LIBC_0001`).
```

```{swr} Supply POSIX.1-2024 XCU encoding, logging, terminal, and user tools
:id: SWR_LIBC_0027
:status: active
:posix: XCU/uuencode, XCU/uudecode, XCU/logger, XCU/tput, XCU/tabs, XCU/mesg
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0050

User space must supply standalone Ring 3 utilities `uuencode`, `uudecode`,
`logger`, `tput`, `tabs`, `mesg`, `write`, and `newgrp` over `libc`, `vfsserv`,
and `ttyserv` ({need}`SYS_LIBC_0001`).
```

```{swr} Supply POSIX.1-2024 XCU scheduling, spooling, and mail utilities
:id: SWR_LIBC_0028
:status: active
:posix: XCU/at, XCU/batch, XCU/crontab, XCU/lp, XCU/mailx (BASE)
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0051

User space must supply standalone Ring 3 utilities `at`, `batch`, `crontab`,
`lp`, and `mailx` over `libc` and `vfsserv` ({need}`SYS_LIBC_0001`).
```

```{swr} Supply XSI syslog, crypt, time, uio, and OpenSSH libc APIs
:id: SWR_LIBC_0029
:status: active
:posix: XBD/syslog.h, XBD/sys/time.h, XBD/sys/uio.h, XSH/crypt (XSI)
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0053

`libc` must supply `<syslog.h>`, `<sys/time.h>`, `<sys/uio.h>`, `crypt`,
`getrusage`, `setpwent`/`getpwent`/`endpwent`, `setgrent`/`getgrent`/`endgrent`,
and `setreuid`/`setregid`/`setresuid`/`setresgid`. It must also supply guarded
OpenSSH compatibility helpers (`vsyslog`, `gettimeofday`, `futimes`,
`setgroups`, `initgroups`, `getgrouplist`, `daemon`, `chroot`)
({need}`SYS_LIBC_0001`).
```

```{swr} Supply cross-compiled Ring 3 sysroot zlib (libz.a and zlib.h)
:id: SWR_LIBC_0030
:status: active
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0055

The CMake build must download and cross-compile `zlib` into the Ring 3 sysroot
(`libz.a`, `<zlib.h>`, and `<zconf.h>`) without storing upstream source in git.
It must support one-shot compression, streaming `deflate`/`inflate`, gzip
framing, `gz*` file I/O, and `crc32`/`adler32` checksums
({need}`SYS_LIBC_0001`).
```

```{swr} Supply cross-compiled Ring 3 sysroot OpenSSL (libcrypto and libssl)
:id: SWR_LIBC_0031
:status: active
:derives: SYS_LIBC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0056

The CMake build must download and cross-compile OpenSSL into the Ring 3 sysroot
(`libcrypto.a`, `libssl.a`, and `<openssl/*.h>`) without storing upstream source
in git. It must support `/dev/urandom` seeding, SHA-256, AES-GCM, RSA, ECDSA,
Ed25519, X.509 certificates, and TLS 1.3 ({need}`SYS_LIBC_0001`).
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
