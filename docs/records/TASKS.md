# Tasks

## Purpose

Track active, temporary engineering tasks (`task`) for `microkernel`.

## Structure

Tasks are ordered by optimal execution order (highest priority first) and link
into the normative model through `:traces:`.

## Formatting Rules

- Normative specifications and records must not link to `TSK_` items.
- Move completed or cancelled tasks verbatim into [HISTORY.md](HISTORY.md) with
  status `done` or `cancelled`.

```{needtable} All tasks
:types: task
:filter: docname == 'records/TASKS'
:columns: id; title; status; traces as "Traces to"; traces_back as "Traced by"
```

### TSK_0033 — Add VFS links, permissions, FIFOs, and <sys/statvfs.h>

```{task} Add VFS links, permissions, FIFOs, and <sys/statvfs.h>
:id: TSK_0033
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_IPC_0001, SYS_LIBC_0001

Add `chmod*`, `chown*`, `faccessat`, `link*`, `symlink*`, `readlink*`, and
`mkfifo*` in `vfsserv` and `pipeserv`. Add `pread`, `pwrite`, `truncate`,
`posix_close`, `posix_getdents`, `scandir`, `alphasort`, `readdir_r`,
`<sys/statvfs.h>`, `remove`, and temp-file helpers. Add file utilities `chmod`,
`chown`, `chgrp`, `ln`, `mkfifo`, `readlink`, `realpath`, and `touch`. Also add
`rmdir`, `df`, `du`, `dd`, `basename`, `dirname`, and `pathchk`
({need}`BDR_0004`, {need}`SYS_IPC_0001`).
```

### TSK_0034 — Add Base signals, waitid, _Fork, popen/system, and <threads.h>

```{task} Add Base signals, waitid, _Fork, popen/system, and <threads.h>
:id: TSK_0034
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_PROC_0001, SYS_SIG_0001

Add `_Fork`, `waitid`, `execl`, `execle`, `execlp`, `fexecve`, `popen`,
`pclose`, and `system`. Add `pause`, `sigsuspend`, `sigwait`, `sigwaitinfo`,
`sigtimedwait`, `sigqueue`, `sig2str`, `str2sig`, `strsignal`, `psignal`, and
`psiginfo`. Supply C11 `<threads.h>`, remaining Base `pthread_*` and `sched_*`
helpers, named `sem_open`/`sem_close`/`sem_unlink`, and `sleep`, `time`,
`timeout`, `nohup`, and `sh` `wait` ({need}`BDR_0004`, {need}`SYS_PROC_0001`,
{need}`SYS_SIG_0001`).
```

### TSK_0035 — Complete Base <stdio.h>, <stdlib.h>, <string.h>, and <time.h>

```{task} Complete Base <stdio.h>, <stdlib.h>, <string.h>, and <time.h>
:id: TSK_0035
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001

Add Base `<stdio.h>` `scanf*`, `getline`, `getdelim`, `dprintf`, `vdprintf`,
`asprintf`, `vasprintf`, `fmemopen`, `open_memstream`, `freopen`, `fgetpos`, and
`fsetpos`. Add `<stdlib.h>` `strtod`/`strtof`/`strtold`/`atof`, `aligned_alloc`,
`quick_exit`/`at_quick_exit`, `div`/`ldiv`/`lldiv`, `rand`/`srand`, `qsort_r`,
and `getsubopt`. Add `stpcpy`, `stpncpy`, `memmem`, `strerror_r`, `time`,
`difftime`, `gmtime*`, `localtime*`, `mktime`, `asctime*`, `ctime*`, `strftime`,
`clock`, and `timespec_get` ({need}`BDR_0004`, {need}`SYS_LIBC_0001`).
```

### TSK_0036 — Add ISO C17 and POSIX.1-2024 <complex.h> and <tgmath.h>

```{task} Add ISO C17 and POSIX.1-2024 <complex.h> and <tgmath.h>
:id: TSK_0036
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001

Supply `<complex.h>` (`CMPLX*`, `cabs*`, `carg*`, `cimag*`, `creal*`, `conj*`,
`cproj*`, `csqrt*`, `cexp*`, `clog*`, `cpow*`, and complex trigonometric and
hyperbolic functions) and `<tgmath.h>` over `<math.h>` with Gates 1 and 2 and
QEMU unit tests ({need}`BDR_0004`, {need}`SYS_LIBC_0001`).
```

### TSK_0037 — Add <locale.h>, <wchar.h>, <wctype.h>, and UTF-8 wide I/O

```{task} Add <locale.h>, <wchar.h>, <wctype.h>, and UTF-8 wide I/O
:id: TSK_0037
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001

Supply `<locale.h>` (`C` and `C.UTF-8` locales, `setlocale`, `newlocale`,
`duplocale`, `freelocale`, `uselocale`, `localeconv`, `getlocalename_l`, and
`*_l` string and character helpers). Supply `<wchar.h>` and `<wctype.h>` with
UTF-8 multibyte conversion, `wcs*`/`wmem*`/`wcsto*`, wide `stdio` streams,
`wcsftime`, `open_wmemstream`, `isw*`, `tow*`, `wctype`, and `wctrans`
({need}`BDR_0004`, {need}`SYS_LIBC_0001`).
```

### TSK_0038 — Add <langinfo.h>, <monetary.h>, <iconv.h>, and <libintl.h>

```{task} Add <langinfo.h>, <monetary.h>, <iconv.h>, and <libintl.h>
:id: TSK_0038
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001

Supply `nl_langinfo`/`nl_langinfo_l`, `strfmon`/`strfmon_l`,
`iconv_open`/`iconv`/`iconv_close`, `<nl_types.h>`
(`catopen`/`catgets`/`catclose`), and Issue 8 Base `<libintl.h>` (`gettext*`,
`ngettext*`, `textdomain`, `bindtextdomain`, `bind_textdomain_codeset`). Add
`locale`, `localedef`, `iconv`, `gencat`, `gettext`, `ngettext`, and `msgfmt`
({need}`BDR_0004`, {need}`SYS_LIBC_0001`).
```

### TSK_0039 — Add <regex.h>, <glob.h>, <wordexp.h>, <tar.h>, and <cpio.h>

```{task} Add <regex.h>, <glob.h>, <wordexp.h>, <tar.h>, and <cpio.h>
:id: TSK_0039
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001

Supply POSIX.1-2024 BRE/ERE matching (`regcomp`, `regexec`, `regerror`,
`regfree`), pathname matching (`glob`, `globfree`), word expansion (`wordexp`,
`wordfree`), and archive format headers `<tar.h>` and `<cpio.h>`
({need}`BDR_0004`, {need}`SYS_LIBC_0001`).
```

### TSK_0040 — Add Ring 3 ELF dynamic symbol resolution and <dlfcn.h>

```{task} Add Ring 3 ELF dynamic symbol resolution and <dlfcn.h>
:id: TSK_0040
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001, SYS_PROC_0001

Add Ring 3 ELF dynamic object loading for `<dlfcn.h>` (`dlopen`, `dlsym`,
`dlclose`, `dlerror`, `dladdr`). Verify with Gates 1 and 2 and QEMU runtime
tests ({need}`BDR_0004`, {need}`SYS_LIBC_0001`).
```

### TSK_0041 — Add Ring 3 virtio-net, TCP/IP stack, and 5 network Base headers

```{task} Add Ring 3 virtio-net, TCP/IP stack, and 5 network Base headers
:id: TSK_0041
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_IPC_0001, SYS_LIBC_0001

Add a Ring 3 PCI `virtio-net` driver and network server with loopback, IPv4,
IPv6, UDP, and TCP. Add `sendmsg`, `recvmsg`, and `sockatmark`. Supply
`<netinet/in.h>`, `<netinet/tcp.h>`, `<arpa/inet.h>`, `<netdb.h>`, and
`<net/if.h>` to reach 70 Base headers ({need}`BDR_0004`, {need}`SYS_IPC_0001`,
{need}`SYS_LIBC_0001`).
```

### TSK_0042 — Complete POSIX.1-2024 XCU shell grammar and built-ins

```{task} Complete POSIX.1-2024 XCU shell grammar and built-ins
:id: TSK_0042
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001

Extend `/boot/sh` with compound commands (`if`, `for`, `while`, `until`, `case`,
subshells), functions, `$(...)` command substitution, `$((...))` arithmetic,
parameter expansions, here-documents, pathname globbing, `trap`, and remaining
standard built-ins (`.`, `:`, `alias`, `unalias`, `command`, `eval`, `exec`,
`getopts`, `hash`, `return`, `shift`, `times`) ({need}`BDR_0004`,
{need}`SYS_LIBC_0001`).
```

### TSK_0043 — Add POSIX.1-2024 XCU text, search, diff, and archive utilities

```{task} Add POSIX.1-2024 XCU text, search, diff, and archive utilities
:id: TSK_0043
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001

Add text utilities `grep`, `sed`, `find`, `xargs`, `sort`, `uniq`, `cut`, `tr`,
`tail`, and `tee`. Add `diff`, `cmp`, `comm`, `paste`, `join`, `printf`, `expr`,
`cksum`, `fold`, `expand`, and `unexpand`. Add `split`, `csplit`, `strings`,
`tsort`, `file`, `pax`, `patch`, and `pr` with QEMU tests ({need}`BDR_0004`,
{need}`SYS_LIBC_0001`).
```

### TSK_0044 — Add remaining POSIX.1-2024 XCU Base utilities

```{task} Add remaining POSIX.1-2024 XCU Base utilities
:id: TSK_0044
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001

Add `awk`, `bc`, `ed`, `m4`, `uuencode`, `uudecode`, `logger`, `tput`, `tabs`,
`mesg`, `write`, and `newgrp`. Also add `at`, `batch`, `crontab`, `lp`, and
`mailx` for the remaining XCU Base set ({need}`BDR_0004`,
{need}`SYS_LIBC_0001`).
```
