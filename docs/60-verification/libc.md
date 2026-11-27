# Verification — LIBC

Domain `LIBC`: Verification items for the freestanding Ring 3 C17 library,
POSIX.1-2024 header rules, and thread-local `errno`.

```{ver} Test of freestanding libc build and Ring 3 operation
:id: VER_LIBC_0001
:status: active
:tags: integration
:verifies: SYS_LIBC_0001, SWR_LIBC_0001

The four-step Clang `-ffreestanding -Wall -Wextra -Werror` build compiles
`libc`. The QEMU test harness `tests/test_runner.py` then starts `sysproc`,
`ipcserv`, `ipccli`, and `sigtest` ({need}`SYS_LIBC_0001`,
{need}`SWR_LIBC_0001`).
```

```{ver} Test of POSIX.1-2024 header matrix and namespace rules
:id: VER_LIBC_0002
:status: active
:tags: unit
:verifies: SWR_LIBC_0002, SWR_LIBC_0003

CTest `posix.headers` starts `PosixHeaderMatrixTests` in
`tests/posix/test_headers.py`. This test compiles each Issue 8 header
independently with `-std=c17 -D_POSIX_C_SOURCE=202405L` and
`-D_XOPEN_SOURCE=800`. It also does a check of `_POSIX_VERSION`, `<unistd.h>`
namespace rules, and `<errno.h>` constants ({need}`SWR_LIBC_0002`,
{need}`SWR_LIBC_0003`).
```

```{ver} Test of thread-local errno in QEMU
:id: VER_LIBC_0003
:status: active
:tags: integration
:verifies: SWR_LIBC_0003

The QEMU test harness `tests/test_runner.py` starts `tests/sigtest/main.c`. This
test makes sure that `errno` starts at `0`, sets `EBADF` on an incorrect
`write`, stays unchanged on success, and stays isolated across threads
({need}`SWR_LIBC_0003`).
```

```{ver} Test of POSIX.1-2024 thread cancellation points and stdio stream locks
:id: VER_LIBC_0004
:status: active
:tags: integration
:verifies: SWR_LIBC_0004

The QEMU test harness `tests/test_runner.py` starts `canceltest`. This test does
a check of `pthread_cancel` on `pthread_cond_wait`, `read`, `ppoll`, `sem_wait`,
and `clock_nanosleep`, LIFO `pthread_cleanup` handlers, and recursive
`flockfile` calls. CTest `posix.headers` does a check of `<pthread.h>` and
`<stdio.h>` ({need}`SWR_LIBC_0004`).
```

```{ver} Test of POSIX.1-2024 Base functions and C17 headers
:id: VER_LIBC_0005
:status: active
:tags: integration
:verifies: SWR_LIBC_0005

The QEMU test harness `tests/test_runner.py` starts `libctest`. This test does a
check of `strlcpy`, `strlcat`, `reallocarray`, `secure_getenv`, `getentropy`,
`<stdatomic.h>`, `<stdalign.h>`, and `<uchar.h>`. CTest `posix.headers` and
`posix.async_signal_safe` compile the headers independently and do a check of
`getentropy` safety ({need}`SWR_LIBC_0005`).
```

```{ver} Test of buffered stdio, env, execvp, and core C17 headers in QEMU
:id: VER_LIBC_0006
:status: active
:tags: integration
:verifies: SWR_LIBC_0007

The QEMU test harness `tests/test_runner.py` starts `stdiotest`. This test does
a check of buffered `fopen`, `fdopen`, `fread`, `fwrite`, `fgets`, `ungetc`,
`fseeko`, and `ftello` on `tmpfs` and `bootfs`. It also does a check of
`setenv`, `unsetenv`, `execvp` `PATH` lookup, `<ctype.h>`, `<strings.h>`,
`<inttypes.h>`, `<limits.h>`, `<assert.h>`, `strdup`, `strstr`, and `strerror`.
CTest `posix.headers`, `posix.symbol_diff`, and `posix.async_signal_safe` do a
check of header rules and `execv` safety ({need}`SWR_LIBC_0007`).
```

```{ver} Test of Ring 3 POSIX.1-2024 XCU shell and core utilities in QEMU
:id: VER_LIBC_0007
:status: active
:tags: integration
:verifies: SWR_LIBC_0006

The QEMU test harness `tests/test_runner.py` starts `shtest`. This test starts
`/boot/sh -c` to do a check of pipelines, redirections, `echo`, `cat`, `ls`,
`true`, `false`, and `test`. It also does a check of `$'...'` syntax, `set -o
pipefail`, built-in commands, and `read -d` with special and NUL delimiters
({need}`SWR_LIBC_0006`).
```

```{ver} Test of more POSIX.1-2024 Base headers and C utilities in QEMU
:id: VER_LIBC_0008
:status: active
:tags: integration
:verifies: SWR_LIBC_0008

The QEMU test harness `tests/test_runner.py` starts `utilstest`
(`tests/utilstest/test_utils.c`). This test does a check of `uname`,
`<endian.h>`, `sched_yield`, `times`, `setjmp`, `sigsetjmp`, `fnmatch`, and
`atexit`. It also does a check of `qsort`, `bsearch`, `strtol`, `strtoul`,
`getopt`, `alarm`, `rename`, `renameat`, and `realpath`. CTest compatibility
gates 1 to 3 do a check of all 42 Base headers and async-signal-safety
({need}`SWR_LIBC_0008`).
```

```{ver} Test of POSIX.1-2024 <fenv.h> and <math.h> with FMA in QEMU
:id: VER_LIBC_0009
:status: active
:tags: integration
:verifies: SWR_LIBC_0009

The QEMU test harness `tests/test_runner.py` starts `mathtest`
(`tests/mathtest/test_math.c`). This test does a check of `MXCSR` modes and
error indicators, IEEE-754 categories, and hardware `FMA` (`sqrt`, `cbrt`,
`hypot`). It also does a check of `exp`, `log`, `pow`, `erf`, `erfc`, `tgamma`,
and trigonometric and hyperbolic functions. CTest compatibility gates 1 and 2
compile the headers independently and make sure that the library exports Base
and CX symbols ({need}`SWR_LIBC_0009`).
```

```{ver} Test of Base <stdio.h>, <stdlib.h>, <string.h>, and <time.h>
:id: VER_LIBC_0010
:status: active
:tags: integration
:verifies: SWR_LIBC_0010

The QEMU test harness `tests/test_runner.py` starts `stdiotest`
(`tests/stdiotest/test_stdio.c`) and `timetest` (`tests/timetest/main.c`). These
tests do a check of `scanf*`, `getline`, `getdelim`, `dprintf*`, `asprintf*`,
`fmemopen`, `open_memstream`, `freopen`, `fgetpos`, and `fsetpos`. They also do
a check of `strtod*`, `aligned_alloc`, `quick_exit`, `at_quick_exit`, `div*`,
`rand`, `srand`, `qsort_r`, `getsubopt`, `stpcpy`, `stpncpy`, `memmem`,
`strerror_r`, and `<time.h>` calendar functions. CTest compatibility gates 1 to
3 compile the headers independently and do a check of Base and CX symbols and
async-signal-safety ({need}`SWR_LIBC_0010`).
```

```{ver} Test of ISO C17 and POSIX.1-2024 <complex.h> and <tgmath.h> in QEMU
:id: VER_LIBC_0011
:status: active
:tags: integration
:verifies: SWR_LIBC_0011

The QEMU test harness `tests/test_runner.py` starts `mathtest`
(`tests/mathtest/test_math.c`). This test does a check of `<complex.h>` math,
`cabs*`, `carg*`, `conj*`, `cproj*`, `csqrt*`, `cexp*`, `clog*`, and `cpow*`. It
also does a check of trigonometric and hyperbolic functions and `<tgmath.h>`
`_Generic` type dispatch. CTest compatibility gates 1 and 2 compile the headers
independently and do a check of all 69 Base and CX symbols
({need}`SWR_LIBC_0011`).
```

```{ver} Test of Base futimens, utimensat, lgamma*, tzset, and tzname in QEMU
:id: VER_LIBC_0012
:status: active
:tags: integration
:verifies: SWR_LIBC_0012

The QEMU test harness `tests/test_runner.py` starts `vfstest`
(`tests/vfstest/test_vfs.c`), `mathtest` (`tests/mathtest/test_math.c`), and
`timetest` (`tests/timetest/main.c`). These tests do a check of `futimens`,
`utimensat` (`UTIME_NOW`, `UTIME_OMIT`, `AT_SYMLINK_NOFOLLOW`, `EROFS`),
`lgamma*` values and poles, `tzset`, and `tzname`. CTest compatibility gates 1
to 3 compile the headers independently and do a check of XSI boundaries, Base
symbols, and async-signal-safety ({need}`SWR_LIBC_0012`).
```

```{ver} Test of <locale.h>, *_l helpers, multibyte, and wchar and wctype
:id: VER_LIBC_0013
:status: active
:tags: integration
:verifies: SWR_LIBC_0013

The QEMU test harness `tests/test_runner.py` starts `libctest`
(`tests/libctest/test_libc_issue8.c`). This test does a check of `C` and
`C.UTF-8` locales, thread-local `uselocale`, `getlocalename_l`, `*_l` helpers,
and UTF-8 multibyte conversions. It also does a check of `<wctype.h>`, `wmem*`,
`wcs*`, `wcsto*`, and `wcsftime`. CTest compatibility gates 1 to 3 compile the
headers independently and do a check of Base symbols and async-signal-safety
({need}`SWR_LIBC_0013`).
```

```{ver} Test of wide-oriented FILE I/O, *wprintf, *wscanf, and open_wmemstream
:id: VER_LIBC_0014
:status: active
:tags: integration
:verifies: SWR_LIBC_0014

The QEMU test harness `tests/test_runner.py` starts `stdiotest`
(`tests/stdiotest/test_stdio.c`). This test does a check of `fwide`, `fgetwc`,
`fputwc`, `ungetwc`, `fgetws`, `fputws`, `fwprintf`, `swprintf`, and `vswprintf`
(with `EOVERFLOW`). It also does a check of `fwscanf`, `swscanf`, `vswscanf`,
and `open_wmemstream`. CTest compatibility gates 1 and 2 compile `<wchar.h>`
independently and do a check of all 23 Base symbols ({need}`SWR_LIBC_0014`).
```

```{ver} Test of <regex.h>, <glob.h>, <wordexp.h>, <tar.h>, and <cpio.h>
:id: VER_LIBC_0015
:status: active
:tags: integration
:verifies: SWR_LIBC_0015

The QEMU test harness `tests/test_runner.py` starts `utilstest`
(`tests/utilstest/test_utils.c`). This test does a check of BRE and ERE regular
expressions, `glob` options and sort sequence, `wordexp` expansions and error
codes, and `<tar.h>` and `<cpio.h>` constants. CTest compatibility gates 1 and 2
compile the headers independently and do a check of all eight Base symbols
({need}`SWR_LIBC_0015`).
```

```{ver} Test of i18n headers, message catalogs, iconv, and i18n CLI utilities
:id: VER_LIBC_0016
:status: active
:tags: integration
:verifies: SWR_LIBC_0016

The QEMU test harness `tests/test_runner.py` starts `utilstest`
(`tests/utilstest/test_utils.c`). This test does a check of `nl_langinfo*`,
`strfmon*`, `iconv` conversions, `gencat` with `catopen` and `catgets`, and
`msgfmt` with `<libintl.h>` plural rules. It also does a check of the `locale`,
`localedef`, `iconv`, `gettext`, and `ngettext` utilities. CTest compatibility
gates 1 and 2 do a check of all five headers, 25 interfaces, and seven utilities
({need}`SWR_LIBC_0016`).
```

```{ver} Test of Ring 3 ELF64 ET_DYN dynamic load and <dlfcn.h>
:id: VER_LIBC_0017
:status: active
:tags: integration
:verifies: SWR_LIBC_0017

The QEMU test harness `tests/test_runner.py` starts `utilstest`
(`tests/utilstest/test_utils.c`) with the shared object `libdltest.so`
(`tests/utilstest/dlplugin.c`). This test does a check of `dlopen`, W^X
`mprotect` operation, relocations, host `libc` calls, `dlsym`, `dladdr`,
`dlerror`, open counts, and constructors and destructors. CTest compatibility
gates 1 and 2 do a check of `<dlfcn.h>` rules and all five Base symbols
({need}`SWR_LIBC_0017`).
```

```{ver} Test of POSIX.1-2024 XCU shell compound grammar and built-ins
:id: VER_LIBC_0018
:status: active
:tags: integration
:verifies: SWR_LIBC_0018

The QEMU test harness `tests/test_runner.py` starts `shtest`
(`tests/shtest/test_sh.c`). This test does a check of `if`, `for`, `while`,
`until`, `case`, subshells, `{ ... }` groups, functions, `$(...)`, and
`$((...))`. It also does a check of parameter expansions, here-documents,
`<glob.h>` patterns, `trap`, and all standard built-in commands. Compatibility
gate 2 does a check of all seven new Base built-in utilities
({need}`SWR_LIBC_0018`).
```

```{ver} Test of XCU utilities grep, sed, find, xargs, expr, and printf
:id: VER_LIBC_0019
:status: active
:tags: integration
:verifies: SWR_LIBC_0019

The QEMU test harness `tests/test_runner.py` starts `shtest`
(`tests/shtest/test_sh.c`). This test does a check of `grep` (`-E`, `-F`, `-i`,
`-n`, `-v`, `-c`, `-q`), `sed` (`-E`, `s///`, `\1`, `y///`, `$`), `find`
(`-print0`, `-exec`), `xargs` (`-0`, `-n`, `-I`), `expr`, and `printf`.
Compatibility gate 2 does a check of all six Base utilities
({need}`SWR_LIBC_0019`).
```

```{ver} Test of network Base headers, AF_INET and AF_INET6 sockets, and netdb
:id: VER_LIBC_0020
:status: active
:tags: integration
:verifies: SWR_LIBC_0020

The QEMU test harness `tests/test_runner.py` starts `nettest`
(`tests/nettest/test_net.c`). This test does a check of `hton*`, `ntoh*`,
`inet_*`, `if_*`, `getaddrinfo`, `getnameinfo`, `gai_strerror`, and `get*ent`.
It also does a check of `AF_INET` and `AF_INET6` sockets, `sendmsg`, `recvmsg`,
`sockatmark`, and multi-server `select`. CTest compatibility gates 1 to 3 do a
check of all 70 Base headers, 1037 Base interfaces, async-signal-safety, and
cancellation points ({need}`SWR_LIBC_0020`).
```

```{ver} Test of XCU sort, uniq, cut, tr, tail, tee, paste, join, and cksum
:id: VER_LIBC_0021
:status: active
:tags: integration
:verifies: SWR_LIBC_0021

The QEMU test harness `tests/test_runner.py` starts `shtest`
(`tests/shtest/test_sh.c`). This test does a check of `sort` (`-t`, `-k`, `-n`,
`-u`, `-c`), `uniq` (`-c`), `cut` (`-d`, `-f`, `-s`, `-c`), and `tr` (character
classes, `-d`, `-s`). It also does a check of `tail` (`-n`, `+N`, `-c`), `tee`,
`paste` (`-s`, `-d`), `join` (`-a`, `-e`, `-o`), and `cksum` CRC-32 values.
Compatibility gate 2 does a check of all nine Base utilities
({need}`SWR_LIBC_0021`).
```

```{ver} Test of XCU diff, cmp, comm, and patch utilities
:id: VER_LIBC_0022
:status: active
:tags: integration
:verifies: SWR_LIBC_0022

The QEMU test harness `tests/test_runner.py` starts `shtest`
(`tests/shtest/test_sh.c`). This test does a check of `cmp` (`-s`), `comm`
(`-12`, `-23`, `-13`), and `diff` and `patch` across unified (`-u`), usual,
context (`-c`), and `-R` formats. Compatibility gate 2 does a check of all four
Base utilities ({need}`SWR_LIBC_0022`).
```

```{ver} Test of XCU pax archive utility (ustar, cpio, -s, and -rw copy mode)
:id: VER_LIBC_0023
:status: active
:tags: integration
:verifies: SWR_LIBC_0023

The QEMU test harness `tests/test_runner.py` starts `shtest`
(`tests/shtest/test_sh.c`). This test makes sure that `pax` makes and reads
`ustar` and `cpio` archives and keeps symlinks. It also does a check of `-s`
regex path changes and `-rw` copy mode. Compatibility gate 2 does a check of the
Base `pax` utility ({need}`SWR_LIBC_0023`).
```

```{ver} Test of XCU split, format, and file-check utilities
:id: VER_LIBC_0024
:status: active
:tags: integration
:verifies: SWR_LIBC_0024

The QEMU test harness `tests/test_runner.py` starts `shtest`
(`tests/shtest/test_sh.c`). This test does a check of `split`, `csplit`, `fold`,
`expand`, `unexpand`, `strings`, `tsort`, `file`, and `pr`. Compatibility gate 2
does a check of all nine Base utilities ({need}`SWR_LIBC_0024`).
```

```{ver} Test of XCU awk language and ed line editor
:id: VER_LIBC_0025
:status: active
:tags: integration
:verifies: SWR_LIBC_0025

The QEMU test harness `tests/test_runner.py` starts `shtest`
(`tests/shtest/test_sh.c`). This test does a check of `awk` field division,
associative arrays, user functions, `<regex.h>`, and `<math.h>`. It also does a
check of `ed` text changes, `g//s`, `u`, and file writes. Compatibility gate 2
does a check of the two Base utilities ({need}`SWR_LIBC_0025`).
```

```{ver} Test of XCU bc calculator and m4 macro processor
:id: VER_LIBC_0026
:status: active
:tags: integration
:verifies: SWR_LIBC_0026

The QEMU test harness `tests/test_runner.py` starts `shtest`
(`tests/shtest/test_sh.c`). This test does a check of `bc` large-precision math,
`scale`, `sqrt`, `obase`, recursive functions, and `-l` math. It also does a
check of `m4` macros, `eval`, `translit`, and diversions. Compatibility gate 2
does a check of the two Base utilities ({need}`SWR_LIBC_0026`).
```

```{ver} Test of XCU encode, logger, terminal, and user utilities
:id: VER_LIBC_0027
:status: active
:tags: integration
:verifies: SWR_LIBC_0027

The QEMU test harness `tests/test_runner.py` starts `shtest`
(`tests/shtest/test_sh.c`). This test does a check of `uuencode` and `uudecode`
(historical and base64), `logger`, `tput`, `tabs`, `mesg`, `write`, and
`newgrp`. Compatibility gate 2 does a check of all eight Base utilities
({need}`SWR_LIBC_0027`).
```

```{ver} Test of XCU at, batch, crontab, lp, and mailx utilities
:id: VER_LIBC_0028
:status: active
:tags: integration
:verifies: SWR_LIBC_0028

The QEMU test harness `tests/test_runner.py` starts `shtest`
(`tests/shtest/test_sh.c`). This test does a check of `at` and `batch` task
queues, `crontab` checks and output, `lp` print queues, and `mailx` send, read,
and remove tasks. Compatibility gate 2 does a check of all 108 Base utilities
({need}`SWR_LIBC_0028`).
```

```{ver} Test of XSI syslog, crypt, time, uio, and OpenSSH libc interfaces
:id: VER_LIBC_0029
:status: active
:tags: integration
:verifies: SWR_LIBC_0029

`utilstest` (`tests/utilstest/test_utils.c`) does a check of `<syslog.h>`,
`crypt`, `gettimeofday`, `utimes`, `futimes`, `readv`, `writev`, and
`getrusage`. It also does a check of `pw` and `gr` iterators, `setresuid`,
`setresgid`, `getgrouplist`, `initgroups`, and `chroot`. Compatibility gate 1
(`tests/posix/test_headers.py`) compiles the headers independently and does a
check of namespace rules ({need}`SWR_LIBC_0029`).
```

```{ver} Test of Ring 3 sysroot zlib in QEMU
:id: VER_LIBC_0030
:status: active
:tags: integration
:verifies: SWR_LIBC_0030

The QEMU test harness `tests/test_runner.py` starts `zlibtest`
(`tests/zlibtest/test_zlib.c`) linked to shared sysroot `libz.so.1`. This test
does a check of `zlibVersion`, `crc32` and `adler32` values, `compress2`,
`uncompress`, and corrupt-data errors. It also does a check of stream zlib and
gzip `deflate` and `inflate`, and `gzopen`, `gzwrite`, and `gzread` on `/tmp`
({need}`SWR_LIBC_0030`).
```

```{ver} Test of Ring 3 sysroot OpenSSL in QEMU
:id: VER_LIBC_0031
:status: active
:tags: integration
:verifies: SWR_LIBC_0031

The QEMU test harness `tests/test_runner.py` starts `openssltest`
(`tests/openssltest/test_openssl.c`) linked to shared sysroot `libssl.so.3` and
`libcrypto.so.3`. This test does a check of `RAND_bytes`, SHA-256, AES-256-GCM,
RSA-2048, ECDSA P-256, Ed25519, X.509 certificates, and a TLS 1.3 handshake on
memory BIO pairs ({need}`SWR_LIBC_0031`).
```

```{ver} Test of the Ring 3 dynamic linker /boot/ld.so in QEMU
:id: VER_LIBC_0032
:status: active
:tags: integration
:verifies: SWR_LIBC_0032

The QEMU test harness starts `dlsoctest`. This test makes a copy of
`libmtest_alt.so` (value 2) to `/tmp/libs/libmtest.so` and starts
`dlsoctest_dyn` with and without `LD_LIBRARY_PATH=/tmp/libs`. The dynamic
executable does a check of `LD_LIBRARY_PATH` priority before `/boot`,
`R_X86_64_64`, `GLOB_DAT`, and `JUMP_SLOT` relocations, data symbols, and
`DT_INIT_ARRAY` constructors ({need}`SWR_LIBC_0032`).
```

```{ver} Test of the shared libc.so dynamic executable in QEMU
:id: VER_LIBC_0033
:status: active
:tags: integration
:verifies: SWR_LIBC_0033

The QEMU test harness starts `dynlibctest`, which starts the `ET_DYN` PIE
executable `dynlibctest_dyn` linked to `libc.so` with `PT_INTERP=/boot/ld.so`.
This executable does a check of `printf`, `snprintf`, `malloc`, `realloc`,
`calloc`, and `free`. It does a check of `dlopen`, `dlsym`, `dlclose`, and
`dlerror` for `libdldyn.so` through `ld.so`. It also does a check of
`pthread_join` with mutexes, thread-local `errno`, and a `SIGUSR1` handler that
keeps `errno` across the signal frame. It makes sure that `fork`, `execve`, and
`waitpid` operate correctly ({need}`SWR_LIBC_0033`).
```

```{ver} Test of the shared sysroot zlib dynamic executable in QEMU
:id: VER_LIBC_0034
:status: active
:tags: integration
:verifies: SWR_LIBC_0034

The QEMU test harness starts `zlibtest` as an `ET_DYN` PIE linked to `libz.so.1`
and `libc.so` with `PT_INTERP=/boot/ld.so`. The interpreter loads `libz.so.1`
from the boot image and resolves its `libc` symbols. This test does a check of
`zlibVersion`, `crc32` and `adler32` values, `compress2`, `uncompress`, stream
zlib and gzip `deflate` and `inflate`, `gzopen`, `gzwrite`, and `gzread`
({need}`SWR_LIBC_0034`).
```

```{ver} Test of the shared sysroot OpenSSL dynamic executable in QEMU
:id: VER_LIBC_0035
:status: active
:tags: integration
:verifies: SWR_LIBC_0035

The QEMU test harness starts `openssltest` as an `ET_DYN` PIE linked to
`libssl.so.3`, `libcrypto.so.3`, and `libc.so` with `PT_INTERP=/boot/ld.so`. The
interpreter loads both shared libraries from the boot image and resolves their
`libc` symbols. This test does a check of `RAND_bytes`, SHA-256, AES-256-GCM,
RSA-2048, ECDSA P-256, Ed25519, X.509 certificates, and a TLS 1.3 handshake on
memory BIO pairs ({need}`SWR_LIBC_0035`).
```

```{ver} Test of the ldd trace mode of /boot/ld.so in QEMU
:id: VER_LIBC_0036
:status: active
:tags: integration
:verifies: SWR_LIBC_0036

The QEMU test harness starts `lddtest` (`tests/lddtest/test_ldd.c`). This test
does a check of the `/usr/bin/ldd` symlink and captures `/boot/ldd` output over
pipes, VFS files, and a PTY for `/boot/zlibtest`, `/boot/openssltest`, and
`/boot/sshd`. It also starts `/boot/zlibtest` with `LD_TRACE_LOADED_OBJECTS=1`
and makes sure that the loader writes `DT_NEEDED` libraries to `fd 1` and exits
0 without starting `main` ({need}`SWR_LIBC_0036`).
```

```{ver} Test of native /usr/sbin/ldconfig in QEMU
:id: VER_LIBC_0037
:status: active
:tags: integration
:verifies: SWR_LIBC_0037

The QEMU test harness starts `lddtest` (`tests/lddtest/test_ldd.c`). This test
does a check of `/usr/sbin/ldconfig` and starts `/boot/ldconfig` on
`/etc/ld.so.conf` defaults and a temporary directory. It makes sure that
`ldconfig` reads `DT_SONAME`, makes the missing symbolic link, and exits 0
({need}`SWR_LIBC_0037`).
```

```{ver} Test of /usr/bin/getent, /etc/nsswitch.conf, and C.UTF-8 locale in QEMU
:id: VER_LIBC_0038
:status: active
:tags: integration
:verifies: SWR_LIBC_0038

The QEMU test harness starts `utilstest` (`tests/utilstest/test_utils.c`). This
test does a check of `libc` NSS lookups, enumeration, and the `/usr/bin/getent`
symlink. It captures `/usr/bin/getent` output over pipes for lookups and full
tables. It reads `/etc/nsswitch.conf` and does a check of `setlocale` and
`nl_langinfo` with `C` and `C.UTF-8` in `libc.a` and `libc.so`
({need}`SWR_LIBC_0038`).
```

```{ver} Test of sysroot IANA tzcode and tzdata in QEMU and CTest
:id: VER_LIBC_0039
:status: active
:tags: integration
:verifies: SWR_LIBC_0039

The QEMU test harness starts `tztest` (`tests/tztest/test_tz.c`). This test runs
`/usr/sbin/zic` on upstream `tzdata` `etcetera` and `australasia`, reads the
`TZif` headers, and captures `/usr/bin/zdump` and `/usr/bin/tzselect` output.
CTest `libc.tzcode` (`tests/boot/test_tzcode.py`) does a check of dynamic
`libc.so` linkage, sysroot files, and `boot.tar` entries
({need}`SWR_LIBC_0039`).
```
