# Interfaces — LIBC

Domain `LIBC`: Public Ring 3 C headers grouped by POSIX.1-2024 interface area.

```{icd} Core freestanding C17 and POSIX base headers
:id: ICD_LIBC_0001
:status: active
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0001, SWR_LIBC_0002, SWR_LIBC_0004

`libc/include/` gives `<stdio.h>` (`FILE`, `flockfile`, `ftrylockfile`,
`funlockfile`), `<stdlib.h>`, `<string.h>`, and `<sys/types.h>` installed into
`sysroot/usr/include/` ({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0001`,
{need}`SWR_LIBC_0002`, {need}`SWR_LIBC_0004`).
```

```{icd} POSIX.1-2024 unistd.h header and feature-test constants
:id: ICD_LIBC_0002
:status: active
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002

`libc/include/unistd.h` declares `_POSIX_VERSION` (`202405L`), `_POSIX2_VERSION`
(`202405L`), descriptor constants, `environ`, and base wrappers with no
`<sys/microkernel.h>` leakage ({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0002`).
```

```{icd} POSIX.1-2024 errno.h and error constants
:id: ICD_LIBC_0003
:status: active
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0003

`libc/include/errno.h` and `libc/include/sys/errno.h` declare POSIX.1-2024 `E*`
error constants and thread-local `errno` ({need}`ARC_LIBC_0001`,
{need}`SWR_LIBC_0002`, {need}`SWR_LIBC_0003`).
```

```{icd} POSIX.1-2024 <uchar.h> and Issue 8 Base library extensions
:id: ICD_LIBC_0004
:status: active
:posix: XSH/getentropy (BASE), XSH/strlcpy (CX), XBD/uchar.h (CX)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0005

`libc/include/uchar.h` declares `char16_t`, `char32_t`, `mbstate_t`, `mbrtoc16`,
`c16rtomb`, `mbrtoc32`, and `c32rtomb`. `<string.h>`, `<stdlib.h>`, and
`<unistd.h>` declare `strlcpy`, `strlcat`, `reallocarray`, `secure_getenv`, and
`getentropy` alongside `<stdatomic.h>` and `<stdalign.h>`
({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0005`).
```

```{icd} POSIX.1-2024 buffered stdio, env, execvp, and core C17 headers
:id: ICD_LIBC_0005
:status: active
:posix: XBD/ctype.h (CX), XBD/limits.h (CX), XSH/fopen (CX), XSH/exec (BASE)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0007

`libc/include/` adds `<ctype.h>`, `<assert.h>`, `<inttypes.h>`, `<strings.h>`,
and `<limits.h>` with POSIX limits. `<stdio.h>`, `<stdlib.h>`, `<string.h>`, and
`<unistd.h>` declare buffered `FILE` operations, `setenv`, `unsetenv`, `execv`,
`execvp`, `strdup`, `strstr`, and `strerror` ({need}`ARC_LIBC_0001`,
{need}`SWR_LIBC_0002`, {need}`SWR_LIBC_0007`).
```

```{icd} POSIX.1-2024 Base utility headers and standard C helpers
:id: ICD_LIBC_0006
:status: active
:posix: XBD/sys/utsname.h (BASE), XBD/endian.h (BASE), XBD/sched.h (BASE)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0008

`libc/include/` adds `<sys/utsname.h>`, `<endian.h>`, `<sched.h>`, `<setjmp.h>`,
`<sys/times.h>`, and `<fnmatch.h>`. `<stdlib.h>`, `<unistd.h>`, and `<stdio.h>`
declare `atexit`, `qsort`, `bsearch`, `realpath`, `getopt`, `alarm`, `rename`,
and `renameat` ({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0002`,
{need}`SWR_LIBC_0008`).
```

```{icd} POSIX.1-2024 <fenv.h> and <math.h> floating-point interfaces
:id: ICD_LIBC_0007
:status: active
:posix: XBD/fenv.h (CX), XBD/math.h (CX)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0009

`libc/include/fenv.h` declares `fenv_t`, `fexcept_t`, `FE_*` macros, and
floating-point control functions. `libc/include/math.h` declares IEEE-754
classification and comparison macros plus the Base/CX `float`, `double`, and
`long double` math functions ({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0002`,
{need}`SWR_LIBC_0009`).
```

```{icd} ISO C17 <threads.h> and Base process/signal/semaphore extensions
:id: ICD_LIBC_0008
:status: active
:posix: XBD/threads.h (CX), XSH/waitid (BASE), XSH/sem_open (BASE)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_PROC_0012

`libc/include/threads.h` declares C11 `thrd_*`, `mtx_*`, `cnd_*`, `call_once`,
and `tss_*` over `<sys/_pthread_types.h>` with no `<pthread.h>` leakage.
`<signal.h>`, `<sys/wait.h>`, `<unistd.h>`, `<stdio.h>`, and `<stdlib.h>`
declare `siginfo_t`, `waitid`, `_Fork`, `execl*`, `fexecve`, `popen`/`pclose`,
and `system`. `<pthread.h>`, `<sched.h>`, and `<semaphore.h>` declare robust
mutexes, `pthread_atfork`, `sched_*`, and named `sem_*` ({need}`ARC_LIBC_0001`,
{need}`SWR_LIBC_0002`, {need}`SWR_PROC_0012`).
```

```{icd} Base <stdio.h>, <stdlib.h>, <string.h>, and <time.h> completion
:id: ICD_LIBC_0009
:status: active
:posix: XBD/stdio.h (CX), XBD/stdlib.h (CX), XBD/string.h (CX), XBD/time.h (CX)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0010

`<stdio.h>` declares `fpos_t`, `scanf*`, `getline`, `getdelim`, `dprintf*`,
`asprintf*`, `fmemopen`, `open_memstream`, `freopen`, and `fgetpos`/`fsetpos`.
`<stdlib.h>` declares `RAND_MAX`, `div_t`/`ldiv_t`/`lldiv_t`, `strtod*`, `atof`,
`aligned_alloc`, `quick_exit`/`at_quick_exit`, `div*`, `rand`/`srand`,
`qsort_r`, and `getsubopt`. `<string.h>` declares `stpcpy`, `stpncpy`, `memmem`,
and `strerror_r`. `<time.h>` declares `struct tm`, `CLOCKS_PER_SEC`, `TIME_UTC`,
`time`, `difftime`, `clock`, `timespec_get`, `gmtime*`, `localtime*`, `mktime`,
`asctime*`, `ctime*`, and `strftime` ({need}`ARC_LIBC_0001`,
{need}`SWR_LIBC_0002`, {need}`SWR_LIBC_0010`).
```

```{icd} ISO C17 and POSIX.1-2024 <complex.h> and <tgmath.h> interfaces
:id: ICD_LIBC_0010
:status: active
:posix: XBD/complex.h (CX), XBD/tgmath.h (CX)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0011

`libc/include/complex.h` declares `complex`, `_Complex_I`, `I`, `CMPLX`,
`CMPLXF`, `CMPLXL`, and all 66 `float`, `double`, and `long double` complex
functions. `libc/include/tgmath.h` provides pure C17 `_Generic` type-generic
macros over `<math.h>` and `<complex.h>` ({need}`ARC_LIBC_0001`,
{need}`SWR_LIBC_0002`, {need}`SWR_LIBC_0011`).
```

```{icd} Base futimens, utimensat, lgamma*, tzset, and tzname interfaces
:id: ICD_LIBC_0011
:status: active
:posix: XBD/sys_stat.h (CX), XBD/math.h (CX), XBD/time.h (CX)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0012

`<sys/stat.h>` declares `UTIME_NOW`, `UTIME_OMIT`, `futimens`, and `utimensat`.
`<math.h>` and `<tgmath.h>` declare `lgamma`, `lgammaf`, `lgammal`, and the
type-generic `lgamma` macro. `<time.h>` declares `tzset` and `extern char
*tzname[2]` without exposing XSI globals under `_POSIX_C_SOURCE=202405L`
({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0002`, {need}`SWR_LIBC_0012`).
```

```{icd} Locale, multibyte, <wctype.h>, and core <wchar.h> interfaces
:id: ICD_LIBC_0012
:status: active
:posix: XBD/locale.h (CX), XBD/wchar.h (CX), XBD/wctype.h (CX)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0013

`<locale.h>` declares `LC_*`, `LC_GLOBAL_LOCALE`, `locale_t`, `struct lconv`,
`setlocale`, `localeconv`, `newlocale`, `duplocale`, `freelocale`, `uselocale`,
and `getlocalename_l`. `<ctype.h>`, `<strings.h>`, `<string.h>`, and `<time.h>`
declare `*_l` helpers, `strcoll*`, and `strxfrm*`. `<stdlib.h>`, `<wctype.h>`,
`<wchar.h>`, and `<inttypes.h>` declare `MB_CUR_MAX`, multibyte functions,
`isw*`/`tow*`/`wctype*`/`wctrans*`, `wmem*`, `wcs*`, `wcsto*`, `wcsftime`, and
`wcstoimax`/`wcstoumax` ({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0002`,
{need}`SWR_LIBC_0013`).
```

```{icd} Wide-oriented FILE streams, *wprintf, *wscanf, and open_wmemstream
:id: ICD_LIBC_0013
:status: active
:posix: XBD/wchar.h (CX), XBD/stdio.h (CX)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0014

`<wchar.h>` declares `FILE` (shared via `_FILE_DEFINED`), `fwide`,
`open_wmemstream`, `fgetwc`, `fgetws`, `fputwc`, `fputws`, `getwc`, `getwchar`,
`putwc`, `putwchar`, and `ungetwc`. It also declares `fwprintf`, `wprintf`,
`swprintf`, `vfwprintf`, `vwprintf`, `vswprintf`, `fwscanf`, `wscanf`,
`swscanf`, `vfwscanf`, `vwscanf`, and `vswscanf` without leaking `<stdio.h>`
macros ({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0002`, {need}`SWR_LIBC_0014`).
```

```{icd} Pattern matching, word expansion, and archive format headers
:id: ICD_LIBC_0014
:status: active
:posix: XBD/regex.h, XBD/glob.h, XBD/wordexp.h, XBD/tar.h, XBD/cpio.h (BASE)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0015

`<regex.h>` declares `regoff_t`, `regex_t`, `regmatch_t`, `REG_*` constants,
`regcomp`, `regexec`, `regerror`, and `regfree`. `<glob.h>` declares `glob_t`,
`GLOB_*` constants, `glob`, and `globfree`. `<wordexp.h>` declares `wordexp_t`,
`WRDE_*` constants, `wordexp`, and `wordfree`. `<tar.h>` and `<cpio.h>` define
POSIX.1-2024 `ustar` and `cpio` header macros ({need}`ARC_LIBC_0001`,
{need}`SWR_LIBC_0002`, {need}`SWR_LIBC_0015`).
```

```{icd} Language info, monetary, message catalog, iconv, and libintl headers
:id: ICD_LIBC_0015
:status: active
:posix: XBD/langinfo.h, monetary.h, nl_types.h, iconv.h, libintl.h (BASE)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0016

`<langinfo.h>` declares `nl_langinfo`, `nl_langinfo_l`, `CODESET`, and
`ALTMON_*`/`ABALTMON_*`. `<monetary.h>` declares `strfmon` and `strfmon_l`.
`<nl_types.h>` declares `nl_item`, `nl_catd`, `catopen`, `catgets`, and
`catclose`. `<iconv.h>` declares `iconv_t`, `iconv_open`, `iconv`, and
`iconv_close`. `<libintl.h>` declares all 15 `gettext*`, `ngettext*`, and
`*textdomain*` interfaces ({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0002`,
{need}`SWR_LIBC_0016`).
```

```{icd} Dynamic linking header <dlfcn.h>
:id: ICD_LIBC_0016
:status: active
:posix: XBD/dlfcn.h (BASE)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0017

`<dlfcn.h>` declares `RTLD_LAZY`, `RTLD_NOW`, `RTLD_GLOBAL`, `RTLD_LOCAL`,
`RTLD_NEXT`, `RTLD_DEFAULT`, `Dl_info_t`, `dlopen`, `dlsym`, `dlclose`,
`dlerror`, and `dladdr` without leaking `<sys/mman.h>` or kernel symbols
({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0002`, {need}`SWR_LIBC_0017`).
```

```{icd} POSIX.1-2024 network Base headers and extended <sys/socket.h>
:id: ICD_LIBC_0017
:status: active
:posix: XBD/netinet/in.h, netinet/tcp.h, arpa/inet.h, net/if.h, netdb.h (BASE)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0020

`<sys/socket.h>` adds `AF_INET`, `AF_INET6`, `struct iovec`, `struct msghdr`,
`struct cmsghdr`, `CMSG_*`, `sendmsg`, `recvmsg`, and `sockatmark`.
`<netinet/in.h>`, `<netinet/tcp.h>`, `<arpa/inet.h>`, `<net/if.h>`, and
`<netdb.h>` declare all 37 remaining Base networking interfaces with zero
`<sys/net.h>` or kernel leakage ({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0002`,
{need}`SWR_LIBC_0020`).
```
