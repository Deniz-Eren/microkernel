# Verification — LIBC

Domain `LIBC`: Verification items covering the freestanding Ring 3 C17 library,
POSIX.1-2024 header hygiene, and thread-local `errno`.

```{ver} Verify freestanding libc build and Ring 3 runtime execution
:id: VER_LIBC_0001
:status: active
:tags: integration
:verifies: SYS_LIBC_0001, SWR_LIBC_0001

Verified by the four-stage Clang `-ffreestanding -Wall -Wextra -Werror` build
and headless QEMU execution of `sysproc`, `ipcserv`, `ipccli`, and `sigtest`
through `tests/test_runner.py` ({need}`SYS_LIBC_0001`, {need}`SWR_LIBC_0001`).
```

```{ver} Verify standalone POSIX.1-2024 header matrix and namespace hygiene
:id: VER_LIBC_0002
:status: active
:tags: unit
:verifies: SWR_LIBC_0002, SWR_LIBC_0003

CTest `posix.headers` runs `PosixHeaderMatrixTests` in
`tests/posix/test_headers.py`. It compiles each Issue 8 header standalone under
`-std=c17 -D_POSIX_C_SOURCE=202405L` and `-D_XOPEN_SOURCE=800`. It also checks
`_POSIX_VERSION`, `<unistd.h>` namespace isolation, and `<errno.h>` constants
({need}`SWR_LIBC_0002`, {need}`SWR_LIBC_0003`).
```

```{ver} Verify thread-local errno isolation and preservation in QEMU
:id: VER_LIBC_0003
:status: active
:tags: integration
:verifies: SWR_LIBC_0003

The headless QEMU harness `tests/test_runner.py` runs `tests/sigtest/main.c`. It
checks that `errno` starts at `0`, records `EBADF` on bad `write`, stays intact
on success, and stays isolated across threads ({need}`SWR_LIBC_0003`).
```

```{ver} Verify POSIX.1-2024 thread cancellation points and stdio stream locks
:id: VER_LIBC_0004
:status: active
:tags: integration
:verifies: SWR_LIBC_0004

The headless QEMU harness `tests/test_runner.py` runs `canceltest`. It checks
`pthread_cancel` on `pthread_cond_wait`, `read`, `ppoll`, `sem_wait`, and
`clock_nanosleep`, LIFO cleanup handlers, and recursive `flockfile` locking.
CTest `posix.headers` checks `<pthread.h>` and `<stdio.h>`
({need}`SWR_LIBC_0004`).
```

```{ver} Verify POSIX.1-2024 Base functions and C17 headers
:id: VER_LIBC_0005
:status: active
:tags: integration
:verifies: SWR_LIBC_0005

The headless QEMU harness `tests/test_runner.py` runs `libctest`. It checks
`strlcpy`, `strlcat`, `reallocarray`, `secure_getenv`, `getentropy`,
`<stdatomic.h>`, `<stdalign.h>`, and `<uchar.h>`. CTest `posix.headers` and
`posix.async_signal_safe` check standalone compilation and `getentropy` safety
({need}`SWR_LIBC_0005`).
```

```{ver} Verify buffered stdio, env, execvp, and core C17 headers in QEMU
:id: VER_LIBC_0006
:status: active
:tags: integration
:verifies: SWR_LIBC_0007

The headless QEMU harness `tests/test_runner.py` runs `stdiotest`. It checks
buffered `fopen`, `fdopen`, `fread`, `fwrite`, `fgets`, `ungetc`, `fseeko`, and
`ftello` over `tmpfs` and `bootfs`. It also checks `setenv`/`unsetenv`, `execvp`
`PATH` lookup, `<ctype.h>`, `<strings.h>`, `<inttypes.h>`, `<limits.h>`,
`<assert.h>`, `strdup`, `strstr`, and `strerror`. CTest `posix.headers`,
`posix.symbol_diff`, and `posix.async_signal_safe` check header hygiene and
`execv` safety ({need}`SWR_LIBC_0007`).
```

```{ver} Verify Ring 3 POSIX.1-2024 XCU shell and core utilities in QEMU
:id: VER_LIBC_0007
:status: active
:tags: integration
:verifies: SWR_LIBC_0006

The headless QEMU harness `tests/test_runner.py` runs `shtest`. It invokes
`/boot/sh -c` to verify pipelines, redirections, `echo`, `cat`, `ls`, `true`,
`false`, `test`, `$'...'` quoting, `set -o pipefail`, built-ins, and `read -d`
with custom and NUL delimiters ({need}`SWR_LIBC_0006`).
```

```{ver} Verify additional POSIX.1-2024 Base headers and C utilities in QEMU
:id: VER_LIBC_0008
:status: active
:tags: integration
:verifies: SWR_LIBC_0008

The headless QEMU harness `tests/test_runner.py` runs `utilstest`
(`tests/utilstest/test_utils.c`). It checks `uname`, `<endian.h>`,
`sched_yield`, `times`, `setjmp`/`sigsetjmp`, `fnmatch`, `atexit`,
`qsort`/`bsearch`, `strtol`/`strtoul`, `getopt`, `alarm`, `rename`/`renameat`,
and `realpath`. CTest Gates 1 to 3 verify all 42 Base headers and
async-signal-safety ({need}`SWR_LIBC_0008`).
```

```{ver} Verify POSIX.1-2024 <fenv.h> and <math.h> with FMA in QEMU
:id: VER_LIBC_0009
:status: active
:tags: integration
:verifies: SWR_LIBC_0009

The headless QEMU harness `tests/test_runner.py` runs `mathtest`
(`tests/mathtest/test_math.c`). It checks `MXCSR` rounding modes, exception
flags, IEEE-754 classification, and exact hardware `FMA` (`sqrt`, `cbrt`,
`hypot`). It also checks `exp`/`log`/`pow`, `erf`/`erfc`, `tgamma`,
trigonometric, inverse trigonometric, and hyperbolic functions. CTest Gates 1
and 2 check standalone compilation and Base/CX symbol export
({need}`SWR_LIBC_0009`).
```

```{ver} Verify Base <stdio.h>, <stdlib.h>, <string.h>, and <time.h> completion
:id: VER_LIBC_0010
:status: active
:tags: integration
:verifies: SWR_LIBC_0010

The headless QEMU harness `tests/test_runner.py` runs `stdiotest`
(`tests/stdiotest/test_stdio.c`) and `timetest` (`tests/timetest/main.c`). They
check `scanf*`, `getline`/`getdelim`, `dprintf*`, `asprintf*`, `fmemopen`,
`open_memstream`, `freopen`, and `fgetpos`/`fsetpos`. They also check `strtod*`,
`aligned_alloc`, `quick_exit`/`at_quick_exit`, `div*`, `rand`/`srand`,
`qsort_r`, `getsubopt`, `stpcpy`/`stpncpy`, `memmem`, `strerror_r`, and
`<time.h>` calendar functions. CTest Gates 1 to 3 check standalone headers,
Base/CX exports, and async-signal-safety ({need}`SWR_LIBC_0010`).
```

```{ver} Verify ISO C17 and POSIX.1-2024 <complex.h> and <tgmath.h> in QEMU
:id: VER_LIBC_0011
:status: active
:tags: integration
:verifies: SWR_LIBC_0011

The headless QEMU harness `tests/test_runner.py` runs `mathtest`
(`tests/mathtest/test_math.c`). It checks `<complex.h>` arithmetic, `cabs*`,
`carg*`, `conj*`, `cproj*`, `csqrt*`, `cexp*`, `clog*`, `cpow*`, trig/hyperbolic
round-trips, and `<tgmath.h>` `_Generic` type dispatch. CTest Gates 1 and 2
check standalone compilation and all 69 Base/CX exports ({need}`SWR_LIBC_0011`).
```

```{ver} Verify Base futimens, utimensat, lgamma*, tzset, and tzname in QEMU
:id: VER_LIBC_0012
:status: active
:tags: integration
:verifies: SWR_LIBC_0012

The headless QEMU harness `tests/test_runner.py` runs `vfstest`
(`tests/vfstest/test_vfs.c`), `mathtest` (`tests/mathtest/test_math.c`), and
`timetest` (`tests/timetest/main.c`). They check `futimens`/`utimensat`
(`UTIME_NOW`, `UTIME_OMIT`, `AT_SYMLINK_NOFOLLOW`, `EROFS`), `lgamma*` values
and poles, and `tzset`/`tzname`. CTest Gates 1 to 3 check standalone headers,
XSI isolation, Base exports, and async-signal-safety ({need}`SWR_LIBC_0012`).
```

```{ver} Verify <locale.h>, *_l helpers, multibyte, and core wchar/wctype
:id: VER_LIBC_0013
:status: active
:tags: integration
:verifies: SWR_LIBC_0013

The headless QEMU harness `tests/test_runner.py` runs `libctest`
(`tests/libctest/test_libc_issue8.c`). It checks `C`/`C.UTF-8` locales,
per-thread `uselocale`, `getlocalename_l`, `*_l` helpers, UTF-8 multibyte
conversions, `<wctype.h>`, `wmem*`, `wcs*`, `wcsto*`, and `wcsftime`. CTest
Gates 1 to 3 check standalone headers, Base exports, and async-signal-safety
({need}`SWR_LIBC_0013`).
```

```{ver} Verify wide-oriented FILE I/O, *wprintf, *wscanf, and open_wmemstream
:id: VER_LIBC_0014
:status: active
:tags: integration
:verifies: SWR_LIBC_0014

The headless QEMU harness `tests/test_runner.py` runs `stdiotest`
(`tests/stdiotest/test_stdio.c`). It checks `fwide`, `fgetwc`/`fputwc`,
`ungetwc`, `fgetws`/`fputws`, `fwprintf`/`swprintf`/`vswprintf` (including
`EOVERFLOW`), `fwscanf`/`swscanf`/`vswscanf`, and `open_wmemstream`. CTest Gates
1 and 2 check standalone `<wchar.h>` hygiene and all 23 Base exports
({need}`SWR_LIBC_0014`).
```

```{ver} Verify <regex.h>, <glob.h>, <wordexp.h>, <tar.h>, and <cpio.h>
:id: VER_LIBC_0015
:status: active
:tags: integration
:verifies: SWR_LIBC_0015

The headless QEMU harness `tests/test_runner.py` runs `utilstest`
(`tests/utilstest/test_utils.c`). It checks BRE/ERE regex matching, `glob` flags
and sorting, `wordexp` expansions and error flags, and `<tar.h>`/`<cpio.h>`
constants. CTest Gates 1 and 2 check standalone header hygiene and all 8 Base
exports ({need}`SWR_LIBC_0015`).
```

```{ver} Verify i18n headers, message catalogs, iconv, and i18n CLI utilities
:id: VER_LIBC_0016
:status: active
:tags: integration
:verifies: SWR_LIBC_0016

The headless QEMU harness `tests/test_runner.py` runs `utilstest`
(`tests/utilstest/test_utils.c`). It checks `nl_langinfo*`, `strfmon*`, `iconv`
conversions, `gencat` with `catopen`/`catgets`, `msgfmt` with `<libintl.h>`
plural forms, and `locale`, `localedef`, `iconv`, `gettext`, and `ngettext`
CLIs. CTest Gates 1 and 2 check all five headers, 25 interfaces, and seven
utilities ({need}`SWR_LIBC_0016`).
```

```{ver} Verify Ring 3 ELF64 ET_DYN dynamic loading and <dlfcn.h>
:id: VER_LIBC_0017
:status: active
:tags: integration
:verifies: SWR_LIBC_0017

The headless QEMU harness `tests/test_runner.py` runs `utilstest`
(`tests/utilstest/test_utils.c`) with the shared object `libdltest.so`
(`tests/utilstest/dlplugin.c`). It checks `dlopen`, W^X `mprotect` execution,
relocations, host `libc` calls, `dlsym`, `dladdr`, `dlerror`, refcounting, and
constructors/destructors. CTest Gates 1 and 2 verify `<dlfcn.h>` hygiene and all
five Base exports ({need}`SWR_LIBC_0017`).
```

```{ver} Verify POSIX.1-2024 XCU shell compound grammar and built-ins
:id: VER_LIBC_0018
:status: active
:tags: integration
:verifies: SWR_LIBC_0018

The headless QEMU harness `tests/test_runner.py` runs `shtest`
(`tests/shtest/test_sh.c`). It verifies `if`, `for`, `while`, `until`, `case`,
subshells, brace groups, functions, `$(...)`, `$((...))`, parameter expansions,
here-documents, `<glob.h>` globbing, `trap`, and all standard built-ins. Gate 2
verifies all seven new Base built-in utilities ({need}`SWR_LIBC_0018`).
```
