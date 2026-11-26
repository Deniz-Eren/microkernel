# Interfaces — LIBC

The `LIBC` domain includes public Ring 3 C headers in POSIX.1-2024 interface
areas.

```{icd} Primary freestanding C17 and POSIX base headers
:id: ICD_LIBC_0001
:status: active
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0001, SWR_LIBC_0002, SWR_LIBC_0004

`libc/include/` gives `<stdio.h>` (`FILE`, `flockfile`, `ftrylockfile`,
`funlockfile`), `<stdlib.h>`, `<string.h>`, and `<sys/types.h>`, and the build
installs them in `sysroot/usr/include/` ({need}`ARC_LIBC_0001`,
{need}`SWR_LIBC_0001`, {need}`SWR_LIBC_0002`, {need}`SWR_LIBC_0004`).
```

```{icd} POSIX.1-2024 unistd.h header and feature-test constants
:id: ICD_LIBC_0002
:status: active
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002

`libc/include/unistd.h` gives `_POSIX_VERSION` (`202405L`), `_POSIX2_VERSION`
(`202405L`), descriptor constants, `environ`, and base wrappers. It does not
include `<sys/microkernel.h>` ({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0002`).
```

```{icd} POSIX.1-2024 errno.h and error constants
:id: ICD_LIBC_0003
:status: active
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0003

`libc/include/errno.h` and `libc/include/sys/errno.h` give POSIX.1-2024 `E*`
error constants and thread-local `errno` ({need}`ARC_LIBC_0001`,
{need}`SWR_LIBC_0002`, {need}`SWR_LIBC_0003`).
```

```{icd} POSIX.1-2024 <uchar.h> and Issue 8 Base library extensions
:id: ICD_LIBC_0004
:status: active
:posix: XSH/getentropy (BASE), XSH/strlcpy (CX), XBD/uchar.h (CX)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0005

`libc/include/uchar.h` gives `char16_t`, `char32_t`, `mbstate_t`, `mbrtoc16`,
`c16rtomb`, `mbrtoc32`, and `c32rtomb`. `<string.h>`, `<stdlib.h>`, and
`<unistd.h>` give `strlcpy`, `strlcat`, `reallocarray`, `secure_getenv`, and
`getentropy` with `<stdatomic.h>` and `<stdalign.h>` ({need}`ARC_LIBC_0001`,
{need}`SWR_LIBC_0005`).
```

```{icd} POSIX.1-2024 buffered stdio, env, execvp, and primary C17 headers
:id: ICD_LIBC_0005
:status: active
:posix: XBD/ctype.h (CX), XBD/limits.h (CX), XSH/fopen (CX), XSH/exec (BASE)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0007

`libc/include/` gives `<ctype.h>`, `<assert.h>`, `<inttypes.h>`, `<strings.h>`,
and `<limits.h>` with POSIX limits. `<stdio.h>`, `<stdlib.h>`, `<string.h>`, and
`<unistd.h>` give buffered `FILE` operations, `setenv`, `unsetenv`, `execv`,
`execvp`, `strdup`, `strstr`, and `strerror` ({need}`ARC_LIBC_0001`,
{need}`SWR_LIBC_0002`, {need}`SWR_LIBC_0007`).
```

```{icd} POSIX.1-2024 Base utility headers and standard C helpers
:id: ICD_LIBC_0006
:status: active
:posix: XBD/sys/utsname.h (BASE), XBD/endian.h (BASE), XBD/sched.h (BASE)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0008

`libc/include/` gives `<sys/utsname.h>`, `<endian.h>`, `<sched.h>`,
`<setjmp.h>`, `<sys/times.h>`, and `<fnmatch.h>`. `<stdlib.h>`, `<unistd.h>`,
and `<stdio.h>` give `atexit`, `qsort`, `bsearch`, `realpath`, `getopt`,
`alarm`, `rename`, and `renameat` ({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0002`,
{need}`SWR_LIBC_0008`).
```

```{icd} POSIX.1-2024 <fenv.h> and <math.h> IEEE-754 interfaces
:id: ICD_LIBC_0007
:status: active
:posix: XBD/fenv.h (CX), XBD/math.h (CX)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0009

`libc/include/fenv.h` gives `fenv_t`, `fexcept_t`, `FE_*` macros, and IEEE-754
control functions. `libc/include/math.h` gives IEEE-754 class and compare macros
and the Base/CX `float`, `double`, and `long double` math functions
({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0002`, {need}`SWR_LIBC_0009`).
```

```{icd} ISO C17 <threads.h> and Base process/signal/semaphore extensions
:id: ICD_LIBC_0008
:status: active
:posix: XBD/threads.h (CX), XSH/waitid (BASE), XSH/sem_open (BASE)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_PROC_0012

`libc/include/threads.h` gives C11 `thrd_*`, `mtx_*`, `cnd_*`, `call_once`, and
`tss_*` on `<sys/_pthread_types.h>`, and does not include `<pthread.h>`.
`<signal.h>`, `<sys/wait.h>`, `<unistd.h>`, `<stdio.h>`, and `<stdlib.h>` give
`siginfo_t`, `waitid`, `_Fork`, `execl*`, `fexecve`, `popen`/`pclose`, and
`system`. `<pthread.h>`, `<sched.h>`, and `<semaphore.h>` give robust mutexes,
`pthread_atfork`, `sched_*`, and named `sem_*` ({need}`ARC_LIBC_0001`,
{need}`SWR_LIBC_0002`, {need}`SWR_PROC_0012`).
```

```{icd} Base <stdio.h>, <stdlib.h>, <string.h>, and <time.h> full interfaces
:id: ICD_LIBC_0009
:status: active
:posix: XBD/stdio.h (CX), XBD/stdlib.h (CX), XBD/string.h (CX), XBD/time.h (CX)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0010

`<stdio.h>` gives `fpos_t`, `scanf*`, `getline`, `getdelim`, `dprintf*`,
`asprintf*`, `fmemopen`, `open_memstream`, `freopen`, and `fgetpos`/`fsetpos`.
`<stdlib.h>` gives `RAND_MAX`, `div_t`/`ldiv_t`/`lldiv_t`, `strtod*`, `atof`,
`aligned_alloc`, `quick_exit`/`at_quick_exit`, `div*`, `rand`/`srand`,
`qsort_r`, and `getsubopt`. `<string.h>` gives `stpcpy`, `stpncpy`, `memmem`,
and `strerror_r`. `<time.h>` gives `struct tm`, `CLOCKS_PER_SEC`, `TIME_UTC`,
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

`libc/include/complex.h` gives `complex`, `_Complex_I`, `I`, `CMPLX`, `CMPLXF`,
`CMPLXL`, and all 66 `float`, `double`, and `long double` complex functions.
`libc/include/tgmath.h` gives C17 `_Generic` type-generic macros for `<math.h>`
and `<complex.h>` ({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0002`,
{need}`SWR_LIBC_0011`).
```

```{icd} Base futimens, utimensat, lgamma*, tzset, and tzname interfaces
:id: ICD_LIBC_0011
:status: active
:posix: XBD/sys_stat.h (CX), XBD/math.h (CX), XBD/time.h (CX)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0012

`<sys/stat.h>` gives `UTIME_NOW`, `UTIME_OMIT`, `futimens`, and `utimensat`.
`<math.h>` and `<tgmath.h>` give `lgamma`, `lgammaf`, `lgammal`, and the
type-generic `lgamma` macro. `<time.h>` gives `tzset` and `extern char
*tzname[2]` and does not show XSI symbols with `_POSIX_C_SOURCE=202405L`
({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0002`, {need}`SWR_LIBC_0012`).
```

```{icd} Locale, multibyte, <wctype.h>, and primary <wchar.h> interfaces
:id: ICD_LIBC_0012
:status: active
:posix: XBD/locale.h (CX), XBD/wchar.h (CX), XBD/wctype.h (CX)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0013

`<locale.h>` gives `LC_*`, `LC_GLOBAL_LOCALE`, `locale_t`, `struct lconv`,
`setlocale`, `localeconv`, `newlocale`, `duplocale`, `freelocale`, `uselocale`,
and `getlocalename_l`. `<ctype.h>`, `<strings.h>`, `<string.h>`, and `<time.h>`
give `*_l` helpers, `strcoll*`, and `strxfrm*`. `<stdlib.h>`, `<wctype.h>`,
`<wchar.h>`, and `<inttypes.h>` give `MB_CUR_MAX`, multibyte functions,
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

`<wchar.h>` gives `FILE` (shared through `_FILE_DEFINED`), `fwide`,
`open_wmemstream`, `fgetwc`, `fgetws`, `fputwc`, `fputws`, `getwc`, `getwchar`,
`putwc`, `putwchar`, and `ungetwc`. It also gives `fwprintf`, `wprintf`,
`swprintf`, `vfwprintf`, `vwprintf`, `vswprintf`, `fwscanf`, `wscanf`,
`swscanf`, `vfwscanf`, `vwscanf`, and `vswscanf`. It does not include
`<stdio.h>` macros ({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0002`,
{need}`SWR_LIBC_0014`).
```

```{icd} Pattern lookup, word expansion, and archive format headers
:id: ICD_LIBC_0014
:status: active
:posix: XBD/regex.h, XBD/glob.h, XBD/wordexp.h, XBD/tar.h, XBD/cpio.h (BASE)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0015

`<regex.h>` gives `regoff_t`, `regex_t`, `regmatch_t`, `REG_*` constants,
`regcomp`, `regexec`, `regerror`, and `regfree`. `<glob.h>` gives `glob_t`,
`GLOB_*` constants, `glob`, and `globfree`. `<wordexp.h>` gives `wordexp_t`,
`WRDE_*` constants, `wordexp`, and `wordfree`. `<tar.h>` and `<cpio.h>` give
POSIX.1-2024 `ustar` and `cpio` header macros ({need}`ARC_LIBC_0001`,
{need}`SWR_LIBC_0002`, {need}`SWR_LIBC_0015`).
```

```{icd} Language info, monetary, message catalog, iconv, and libintl headers
:id: ICD_LIBC_0015
:status: active
:posix: XBD/langinfo.h, monetary.h, nl_types.h, iconv.h, libintl.h (BASE)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0016

`<langinfo.h>` gives `nl_langinfo`, `nl_langinfo_l`, `CODESET`, and
`ALTMON_*`/`ABALTMON_*`. `<monetary.h>` gives `strfmon` and `strfmon_l`.
`<nl_types.h>` gives `nl_item`, `nl_catd`, `catopen`, `catgets`, and `catclose`.
`<iconv.h>` gives `iconv_t`, `iconv_open`, `iconv`, and `iconv_close`.
`<libintl.h>` gives all 15 `gettext*`, `ngettext*`, and `*textdomain*`
interfaces ({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0002`,
{need}`SWR_LIBC_0016`).
```

```{icd} Dynamic link header <dlfcn.h>
:id: ICD_LIBC_0016
:status: active
:posix: XBD/dlfcn.h (BASE)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0017

`<dlfcn.h>` gives `RTLD_LAZY`, `RTLD_NOW`, `RTLD_GLOBAL`, `RTLD_LOCAL`,
`RTLD_NEXT`, `RTLD_DEFAULT`, `Dl_info_t`, `dlopen`, `dlsym`, `dlclose`,
`dlerror`, and `dladdr`. It does not include `<sys/mman.h>` or kernel symbols
({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0002`, {need}`SWR_LIBC_0017`).
```

```{icd} POSIX.1-2024 network Base headers and extended <sys/socket.h>
:id: ICD_LIBC_0017
:status: active
:posix: XBD/netinet/in.h, netinet/tcp.h, arpa/inet.h, net/if.h, netdb.h (BASE)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0020

`<sys/socket.h>` gives `AF_INET`, `AF_INET6`, `struct iovec`, `struct msghdr`,
`struct cmsghdr`, `CMSG_*`, `sendmsg`, `recvmsg`, and `sockatmark`.
`<netinet/in.h>`, `<netinet/tcp.h>`, `<arpa/inet.h>`, `<net/if.h>`, and
`<netdb.h>` give all 37 remaining Base network interfaces and do not include
`<sys/net.h>` or kernel symbols ({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0002`,
{need}`SWR_LIBC_0020`).
```

```{icd} POSIX.1-2024 XSI syslog, time, and uio headers
:id: ICD_LIBC_0018
:status: active
:posix: XBD/syslog.h, XBD/sys/time.h, XBD/sys/uio.h (XSI)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0029

`<syslog.h>` gives `LOG_*`, `openlog`, `syslog`, `closelog`, and `setlogmask`.
`<sys/time.h>` gives `struct timeval` and `utimes`. `<sys/uio.h>` gives `struct
iovec`, `readv`, and `writev`. Non-POSIX helpers stay behind feature macros out
of `_POSIX_C_SOURCE=202405L` views ({need}`ARC_LIBC_0001`,
{need}`SWR_LIBC_0002`, {need}`SWR_LIBC_0029`).
```
