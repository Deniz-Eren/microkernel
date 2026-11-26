# History

## Purpose

This file keeps completed and canceled task and roadmap entries (`task`,
`roadmap`) moved from [TASKS.md](TASKS.md) and [ROADMAP.md](ROADMAP.md).

## Structure

This file keeps entries with newest items first and does not change their
initial IDs or links.

## Format Rules

- Set the status of each archived item to `done` or `cancelled`.
- Do not use IDs from archived entries again.

```{needtable} Completed tasks and roadmap items
:types: task, roadmap
:filter: docname == 'records/HISTORY' and status in ['done', 'cancelled']
:columns: id; title; status; traces as "Traces to"; traces_back as "Traced by"
```

### TSK_0066 — Link Ring 3 bin/ shell and utilities dynamically to libc.so

```{task} Link Ring 3 bin/ shell and utilities dynamically to libc.so
:id: TSK_0066
:status: done
:traces: BDR_0003, BDR_0004, ADR_0002, SYS_LIBC_0001, SYS_PROC_0001

Change `bin/` (`sh`, XCU utilities, `nc`, `ping`, `sshd-setup`) to dynamic
executables linked to `ld.so` and `libc.so`. Keep primary Ring 3 boot servers
statically linked. Make sure that the `boot.tar` byte count decreases. Do the
full QEMU and CTest suites. Make sure that all tests complete without errors
({need}`SYS_LIBC_0001`, {need}`SYS_PROC_0001`).
```

### TSK_0065 — Link OpenSSH Portable dynamically to shared libraries

```{task} Link OpenSSH Portable dynamically to shared libraries
:id: TSK_0065
:status: done
:traces: BDR_0003, BDR_0004, ADR_0054, ADR_0055, ADR_0056, SYS_IPC_0001

Change `cmake/openssh.cmake` to link `sshd`, `sshd-session`, and `ssh-keygen`
dynamically with `--dynamic-linker=/boot/ld.so`. Link those executables to
`libcrypto.so.3`, `libz.so.1`, and `libc.so`. Examine `PT_INTERP` and
`DT_NEEDED` headers with `llvm-readelf`. Do tests of RSA, ECDSA, Ed25519,
compressed (`zlib@openssh.com`), password, and PTY sessions in CTest
`ipc.sshd_e2e` ({need}`SYS_IPC_0001`).
```

### TSK_0064 — Make shared libcrypto.so.3 and libssl.so.3 with CMake

```{task} Make shared libcrypto.so.3 and libssl.so.3 with CMake
:id: TSK_0064
:status: done
:traces: BDR_0003, BDR_0004, ADR_0056, SYS_LIBC_0001

Change `cmake/openssl_target.conf`, `cmake/build_openssl.sh`, and
`cmake/openssl.cmake` to compile `-fPIC` shared `libcrypto.so.3` and
`libssl.so.3`. Make the shared libraries with the static archives during the
CMake build. Do not keep upstream source in git. Install the shared libraries
into the sysroot and `boot.tar`. Link `openssltest` dynamically. Do a test of
cryptography and TLS 1.3 in QEMU ({need}`SYS_LIBC_0001`).
```

### TSK_0063 — Make shared libz.so.1 with CMake and link zlibtest dynamically

```{task} Make shared libz.so.1 with CMake and link zlibtest dynamically
:id: TSK_0063
:status: done
:traces: BDR_0003, BDR_0004, ADR_0055, SYS_LIBC_0001

Change `cmake/zlib.cmake` to compile shared `libz.so.1` (with `libz.so` symlink)
with `libz.a` during the CMake build. Do not keep upstream source in git.
Install `libz.so.1` into the Ring 3 sysroot and `boot.tar`. Link `zlibtest`
dynamically to `libz.so.1` and `libc.so`. Do tests in QEMU
({need}`SYS_LIBC_0001`).
```

### TSK_0062 — Make shared libc.so and unify dlopen/dlsym with ld.so

```{task} Make shared libc.so and unify dlopen/dlsym with ld.so
:id: TSK_0062
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001, SYS_MEM_0001

Compile `-fPIC` shared `libc.so` with static `libc.a` into the Ring 3 sysroot
and `boot.tar`. Change `crt0.S` and the `__libc_start_main` startup interface.
Add a dynamic executable linker script with `FILEHDR PHDRS`. Send `<dlfcn.h>`
runtime calls to `ld.so` for dynamic executables. Do tests of `stdio`, `malloc`,
pthreads, TLS `errno`, signals, `fork`/`execve`, and `dlopen` in QEMU
({need}`SYS_LIBC_0001`, {need}`SYS_MEM_0001`).
```

### RDM_0003 — Target POSIX.1-2024 (Issue 8) Base compatibility

```{roadmap} Target POSIX.1-2024 (Issue 8) Base compatibility
---
id: RDM_0003
status: done
traces: >
  BDR_0003, ADR_0003, SYS_LIBC_0001, SYS_PROC_0001, SYS_SIG_0001,
  SYS_MEM_0001, SYS_IPC_0001
---

This roadmap item targets compatibility with interfaces from POSIX.1-2024 (IEEE
Std 1003.1-2024 / ISO/IEC/IEEE 9945:2026) across headers, `errno`, signals,
`pthread_*`, `sem_*`, clocks, timers, `mmap`, `pipe2`/`dup3`/`ppoll`, and
`aio_*`. It also includes command shell utilities (`$'...'`, `set -o pipefail`,
`read -d`) with automatic compatibility gates ({need}`BDR_0003`,
{need}`ADR_0003`).
```

### TSK_0061 — Add kernel PT_INTERP, SysV auxv, and Ring 3 ld.so

```{task} Add kernel PT_INTERP, SysV auxv, and Ring 3 ld.so
:id: TSK_0061
:status: done
:traces: BDR_0003, BDR_0004, ADR_0002, SYS_BOOT_0001, SYS_PROC_0001

Change `kernel/core/elf.c` and `kernel/core/proc.c` to load `ET_DYN` and
`PT_INTERP` (`/boot/ld.so`) with no new syscalls. Write SysV `auxv` entries
(`AT_PHDR`, `AT_PHENT`, `AT_PHNUM`, `AT_BASE`, `AT_ENTRY`). Make freestanding
Ring 3 `ld.so` to load `DT_NEEDED` libraries through `/dev/vfs` and apply
`R_X86_64_*` relocations. Make `ld.so` obey W^X, start `DT_INIT_ARRAY`, and jump
to `AT_ENTRY`. Do a dynamic linker test in QEMU ({need}`SYS_BOOT_0001`,
{need}`SYS_PROC_0001`).
```

### TSK_0060 — Add OpenSSL and zlib to OpenSSH Portable

```{task} Add OpenSSL and zlib to OpenSSH Portable
:id: TSK_0060
:status: done
:traces: BDR_0003, BDR_0004, ADR_0054, ADR_0055, ADR_0056, SWR_IPC_0016

Change the CMake OpenSSH Portable build configuration to link to the Ring 3
sysroot OpenSSL and `zlib` libraries. Download all third-party sources during
the CMake build. Do not keep upstream source in the repository. Do tests of RSA,
ECDSA, Ed25519, and compressed SSH sessions in CTest ({need}`SWR_IPC_0016`,
{need}`SYS_IPC_0001`, {need}`SYS_LIBC_0001`).
```

### TSK_0059 — Compile OpenSSL into the Ring 3 sysroot with CMake

```{task} Compile OpenSSL into the Ring 3 sysroot with CMake
:id: TSK_0059
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0056, SWR_LIBC_0031

Keep only CMake build configurations in the repository. Download and compile
OpenSSL (`libcrypto.a`, `libssl.a`, and headers) for Ring 3 during the CMake
build. Do not keep upstream source in git. Do tests of SHA-256, RSA, ECDSA, and
TLS primitives in QEMU. Change `docs/THIRD-PARTY-NOTICES.md`
({need}`SWR_LIBC_0031`, {need}`SYS_LIBC_0001`).
```

### TSK_0058 — Compile zlib into the Ring 3 sysroot with CMake

```{task} Compile zlib into the Ring 3 sysroot with CMake
:id: TSK_0058
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0055, SWR_LIBC_0030

Keep only CMake build configurations in the repository. Download and compile
`zlib` for Ring 3 during the CMake build into the Ring 3 sysroot (`libz.a`,
`<zlib.h>`). Do not keep upstream source in git. Do tests of compression in
QEMU. Change `docs/THIRD-PARTY-NOTICES.md` ({need}`SWR_LIBC_0030`,
{need}`SYS_LIBC_0001`).
```

### TSK_0057 — Compile OpenSSH Portable sshd and ssh-keygen for Ring 3

```{task} Compile OpenSSH Portable sshd and ssh-keygen for Ring 3
:id: TSK_0057
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0054, SWR_IPC_0016

Compile OpenSSH Portable (`--without-openssl --without-zlib`) for Ring 3 with
Ed25519 `authorized_keys` and dev password login. Keep only CMake build
configurations in the repository. Download and compile OpenSSH during the CMake
build. Do not keep upstream source in the repository. Start `sshd` on
interactive boot. Do tests of OpenSSH client logins in CTest
({need}`SWR_IPC_0016`, {need}`SYS_IPC_0001`, {need}`SYS_LIBC_0001`).
```

### TSK_0056 — Add Ring 3 PTYs, virtual devices, SCM_RIGHTS, and OpenSSH APIs

```{task} Add Ring 3 PTYs, virtual devices, SCM_RIGHTS, and OpenSSH APIs
:id: TSK_0056
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0053, SWR_IPC_0015, SWR_LIBC_0029

Change `servers/ttyserv/` to add Ring 3 PTYs (`/dev/ptmx`, `/dev/pts/N`) and
`ioctl`. Add `/dev/null`, `/dev/urandom`, Ring 3 `SCM_RIGHTS` descriptor send,
`<syslog.h>`, `crypt`, and OpenSSH `libc` APIs with QEMU tests
({need}`SWR_IPC_0015`, {need}`SWR_LIBC_0029`, {need}`SYS_IPC_0001`,
{need}`SYS_LIBC_0001`, {need}`SYS_PROC_0001`).
```

### TSK_0055 — Add host-to-guest network, netserv changes, `nc`, and `ping`

```{task} Add host-to-guest network, netserv changes, nc, and ping
:id: TSK_0055
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0042, ADR_0052, SWR_IPC_0014

Add localhost QEMU and Podman TCP port maps in `scripts/env.sh`. Change
`servers/netserv/` for inbound host TCP traffic. Add Ring 3 `nc` and `ping`
utilities with an automatic host-to-guest TCP test ({need}`ADR_0042`,
{need}`SWR_IPC_0014`, {need}`SYS_IPC_0001`, {need}`SYS_LIBC_0001`).
```

### TSK_0053 — Add XCU batch, print, and mail utilities

```{task} Add XCU batch, print, and mail utilities
:id: TSK_0053
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0051, SWR_LIBC_0028

Add `at`, `batch`, `crontab`, `lp`, and `mailx` to complete the remaining XCU
Base utility set ({need}`BDR_0004`, {need}`SWR_LIBC_0028`,
{need}`SYS_LIBC_0001`).
```

### TSK_0048 — Add XCU data, event, terminal, and user utilities

```{task} Add XCU data, event, terminal, and user utilities
:id: TSK_0048
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0050, SWR_LIBC_0027

Add `uuencode`, `uudecode`, `logger`, `tput`, `tabs`, `mesg`, `write`, and
`newgrp` with QEMU runtime tests ({need}`BDR_0004`, {need}`SWR_LIBC_0027`,
{need}`SYS_LIBC_0001`).
```

### TSK_0052 — Add POSIX.1-2024 XCU bc and m4 language utilities

```{task} Add POSIX.1-2024 XCU bc and m4 language utilities
:id: TSK_0052
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0049, SWR_LIBC_0026

Add the arbitrary-precision calculator `bc` and macro processor `m4` with QEMU
runtime tests ({need}`BDR_0004`, {need}`SWR_LIBC_0026`, {need}`SYS_LIBC_0001`).
```

### TSK_0044 — Add POSIX.1-2024 XCU awk and ed utilities

```{task} Add POSIX.1-2024 XCU awk and ed utilities
:id: TSK_0044
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0048, SWR_LIBC_0025

Add the pattern language `awk` and line editor `ed` with `<regex.h>` and
`<math.h>` and QEMU runtime tests ({need}`BDR_0004`, {need}`SWR_LIBC_0025`,
{need}`SYS_LIBC_0001`).
```

### TSK_0051 — Add POSIX.1-2024 XCU text format and file utilities

```{task} Add POSIX.1-2024 XCU text format and file utilities
:id: TSK_0051
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0047, SWR_LIBC_0024

Add text format utilities `split`, `csplit`, `fold`, `expand`, `unexpand`, and
`pr`. Add `strings`, `tsort`, and `file` with QEMU tests ({need}`BDR_0004`,
{need}`SWR_LIBC_0024`, {need}`SYS_LIBC_0001`).
```

### TSK_0054 — Add POSIX.1-2024 XCU pax archive utility

```{task} Add POSIX.1-2024 XCU pax archive utility
:id: TSK_0054
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0046, SWR_LIBC_0023

Add the portable archive utility `pax` with read, write, and copy modes for
`<tar.h>` `ustar` and `<cpio.h>` formats. Add QEMU runtime tests
({need}`BDR_0004`, {need}`SWR_LIBC_0023`, {need}`SYS_LIBC_0001`).
```

### TSK_0046 — Add POSIX.1-2024 XCU diff, cmp, comm, and patch utilities

```{task} Add POSIX.1-2024 XCU diff, cmp, comm, and patch utilities
:id: TSK_0046
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0045, SWR_LIBC_0022

Add file diff and patch utilities `diff`, `cmp`, `comm`, and `patch` with QEMU
runtime tests ({need}`BDR_0004`, {need}`SWR_LIBC_0022`, {need}`SYS_LIBC_0001`).
```

### TSK_0050 — Add POSIX.1-2024 XCU sort, field, and checksum text utilities

```{task} Add POSIX.1-2024 XCU sort, field, and checksum text utilities
:id: TSK_0050
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0044, SWR_LIBC_0021

Add text pipeline utilities `sort`, `uniq`, `cut`, `tr`, `tail`, `tee`, `paste`,
`join`, and `cksum` with QEMU runtime tests ({need}`BDR_0004`,
{need}`SWR_LIBC_0021`, {need}`SYS_LIBC_0001`).
```

### TSK_0047 — Add 5 network Base headers and socket/resolver libc APIs

```{task} Add 5 network Base headers and socket/resolver libc APIs
:id: TSK_0047
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0043, SWR_LIBC_0020

Connect `AF_INET` and `AF_INET6` into `<sys/socket.h>` and `libc` FD dispatch
(`read`, `write`, `close`, `dup*`, `poll`, `select`) through `/dev/net`. Add
`sendmsg`, `recvmsg`, and `sockatmark`. Add `<netinet/in.h>`, `<netinet/tcp.h>`,
`<arpa/inet.h>`, `<netdb.h>`, and `<net/if.h>` (`htons`/`htonl`/`ntohs`/`ntohl`,
`inet_*`, `if_*`, `getaddrinfo`, `getnameinfo`, `gai_strerror`, `get*ent`) to
get 70 Base headers and 1037 Base interfaces ({need}`BDR_0004`,
{need}`SWR_LIBC_0020`, {need}`SYS_IPC_0001`, {need}`SYS_LIBC_0001`).
```

### TSK_0041 — Add Ring 3 PCI virtio-net driver and TCP/IP/loopback server

```{task} Add Ring 3 PCI virtio-net driver and TCP/IP/loopback server
:id: TSK_0041
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0042, SWR_IPC_0013

Add a Ring 3 PCI `virtio-net` driver with active `CAP_SYS_INTR` MMIO, DMA, and
IOAPIC IRQ pulses. Add a Ring 3 network server with loopback, Ethernet, ARP,
IPv4, IPv6, ICMP, UDP, and TCP. Do tests in QEMU ({need}`BDR_0004`,
{need}`SWR_IPC_0013`, {need}`SYS_IPC_0001`).
```

### TSK_0043 — Add POSIX.1-2024 XCU regex, find, and primary stream utilities

```{task} Add POSIX.1-2024 XCU regex, find, and primary stream utilities
:id: TSK_0043
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0041, SWR_LIBC_0019

Add `grep`, `sed`, `find`, `xargs`, `expr`, and `printf` with `<regex.h>` and
`<fnmatch.h>` and QEMU tests ({need}`BDR_0004`, {need}`SWR_LIBC_0019`,
{need}`SYS_LIBC_0001`).
```

### TSK_0042 — Complete POSIX.1-2024 XCU shell grammar and built-in commands

```{task} Complete POSIX.1-2024 XCU shell grammar and built-in commands
:id: TSK_0042
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0040, SWR_LIBC_0018

Change `/boot/sh` to add compound commands (`if`, `for`, `while`, `until`,
`case`, subshells), functions, `$(...)` command substitution, `$((...))`
arithmetic, parameter expansions, here-documents, `<glob.h>` path expansion, and
`trap`. Add remaining standard built-in commands (`.`, `:`, `alias`, `unalias`,
`command`, `eval`, `exec`, `getopts`, `hash`, `return`, `shift`, `times`)
({need}`BDR_0004`, {need}`SWR_LIBC_0018`, {need}`SYS_LIBC_0001`).
```

### TSK_0040 — Add Ring 3 ELF dynamic symbol lookup and <dlfcn.h>

```{task} Add Ring 3 ELF dynamic symbol lookup and <dlfcn.h>
:id: TSK_0040
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0039, SWR_LIBC_0017

Add Ring 3 ELF dynamic object load (`ET_DYN` with W^X `mmap`/`mprotect`
relocation) for `<dlfcn.h>` (`dlopen`, `dlsym`, `dlclose`, `dlerror`, `dladdr`).
Do tests with Gates 1 and 2 and QEMU runtime tests ({need}`BDR_0004`,
{need}`SWR_LIBC_0017`, {need}`SYS_LIBC_0001`).
```

### TSK_0038 — Add <langinfo.h>, <monetary.h>, <iconv.h>, and <libintl.h>

```{task} Add <langinfo.h>, <monetary.h>, <iconv.h>, and <libintl.h>
:id: TSK_0038
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0038, SWR_LIBC_0016

Add `nl_langinfo`/`nl_langinfo_l`, `strfmon`/`strfmon_l`,
`iconv_open`/`iconv`/`iconv_close`, `<nl_types.h>`
(`catopen`/`catgets`/`catclose`), and Issue 8 Base `<libintl.h>` (`gettext*`,
`ngettext*`, `textdomain`, `bindtextdomain`, `bind_textdomain_codeset`). Add
`locale`, `localedef`, `iconv`, `gencat`, `gettext`, `ngettext`, and `msgfmt`
({need}`BDR_0004`, {need}`SWR_LIBC_0016`, {need}`SYS_LIBC_0001`).
```

### TSK_0039 — Add <regex.h>, <glob.h>, <wordexp.h>, <tar.h>, and <cpio.h>

```{task} Add <regex.h>, <glob.h>, <wordexp.h>, <tar.h>, and <cpio.h>
:id: TSK_0039
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0037, SWR_LIBC_0015

Add POSIX.1-2024 BRE/ERE pattern match (`regcomp`, `regexec`, `regerror`,
`regfree`) and path match (`glob`, `globfree`). Add word expansion (`wordexp`,
`wordfree`) and archive headers `<tar.h>` and `<cpio.h>` ({need}`BDR_0004`,
{need}`SWR_LIBC_0015`, {need}`SYS_LIBC_0001`).
```

### TSK_0045 — Add wide-oriented <stdio.h>/<wchar.h> I/O and open_wmemstream

```{task} Add wide-oriented <stdio.h>/<wchar.h> I/O and open_wmemstream
:id: TSK_0045
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0036, SWR_LIBC_0014

Change Ring 3 `FILE` streams to add wide orientation (`fwide`) and wide
character I/O (`fgetwc`, `fgetws`, `fputwc`, `fputws`, `getwc`, `getwchar`,
`putwc`, `putwchar`, `ungetwc`). Add wide formatted I/O (`fwprintf`, `wprintf`,
`swprintf`, `vfwprintf`, `vwprintf`, `vswprintf`, `fwscanf`, `wscanf`,
`swscanf`, `vfwscanf`, `vwscanf`, `vswscanf`) and `open_wmemstream` with QEMU
tests ({need}`BDR_0004`, {need}`SWR_LIBC_0014`, {need}`SYS_LIBC_0001`).
```

### TSK_0037 — Add <locale.h>, *_l helpers, multibyte, and primary wchar/wctype

```{task} Add <locale.h>, *_l helpers, multibyte, and primary wchar/wctype
:id: TSK_0037
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0035, SWR_LIBC_0013

Add `<locale.h>` (`C` and `C.UTF-8` locales, `setlocale`, `newlocale`,
`duplocale`, `freelocale`, `uselocale`, `localeconv`, `getlocalename_l`). Add
locale helpers `*_l` (`<ctype.h>`, `strcasecmp_l`, `strncasecmp_l`, `strcoll`,
`strcoll_l`, `strxfrm`, `strxfrm_l`, `strerror_l`, `strftime_l`). Add
`<stdlib.h>` `mblen`, `mbtowc`, `wctomb`, `mbstowcs`, and `wcstombs`. Add
`<wctype.h>` (`isw*`, `tow*`, `wctype*`, `wctrans*`) and primary `<wchar.h>`
(`btowc`, `wctob`, `mbr*`, `mbs*`, `wcrtomb`, `wcs*`, `wmem*`, `wcsto*`,
`wcsftime`, plus `<inttypes.h>` `wcstoimax`/`wcstoumax`) ({need}`BDR_0004`,
{need}`SWR_LIBC_0013`, {need}`SYS_LIBC_0001`).
```

### TSK_0049 — Add Base futimens, utimensat, lgamma*, tzset, and tzname

```{task} Add Base futimens, utimensat, lgamma*, tzset, and tzname
:id: TSK_0049
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0034, SWR_LIBC_0012

Add `futimens` and `utimensat` (`UTIME_NOW`, `UTIME_OMIT`) in `<sys/stat.h>` and
Ring 3 `vfsserv`. Add `lgamma`, `lgammaf`, and `lgammal` in `<math.h>` and
`<tgmath.h>`. Add `tzset` and `tzname` in `<time.h>`. Do tests in QEMU and Gates
1 and 2 ({need}`BDR_0004`, {need}`SWR_LIBC_0012`, {need}`SYS_LIBC_0001`,
{need}`SYS_IPC_0001`).
```

### TSK_0036 — Add ISO C17 and POSIX.1-2024 <complex.h> and <tgmath.h>

```{task} Add ISO C17 and POSIX.1-2024 <complex.h> and <tgmath.h>
:id: TSK_0036
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0033, SWR_LIBC_0011

Add `<complex.h>` (`CMPLX*`, `cabs*`, `carg*`, `cimag*`, `creal*`, `conj*`,
`cproj*`, `csqrt*`, `cexp*`, `clog*`, `cpow*`, and complex trigonometric and
hyperbolic functions). Add `<tgmath.h>` for `<math.h>` with Gates 1 and 2 and
QEMU unit tests ({need}`BDR_0004`, {need}`SWR_LIBC_0011`,
{need}`SYS_LIBC_0001`).
```

### TSK_0035 — Complete Base <stdio.h>, <stdlib.h>, <string.h>, and <time.h>

```{task} Complete Base <stdio.h>, <stdlib.h>, <string.h>, and <time.h>
:id: TSK_0035
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0032, SWR_LIBC_0010

Add Base `<stdio.h>` `scanf*`, `getline`, `getdelim`, `dprintf`, `vdprintf`,
`asprintf`, `vasprintf`, `fmemopen`, `open_memstream`, `freopen`, `fgetpos`, and
`fsetpos`. Add `<stdlib.h>` `strtod`/`strtof`/`strtold`/`atof`, `aligned_alloc`,
`quick_exit`/`at_quick_exit`, `div`/`ldiv`/`lldiv`, `rand`/`srand`, `qsort_r`,
and `getsubopt`. Add `stpcpy`, `stpncpy`, `memmem`, `strerror_r`, `time`,
`difftime`, `gmtime*`, `localtime*`, `mktime`, `asctime*`, `ctime*`, `strftime`,
`clock`, and `timespec_get` ({need}`BDR_0004`, {need}`SWR_LIBC_0010`,
{need}`SYS_LIBC_0001`).
```

### TSK_0034 — Add Base signals, waitid, _Fork, popen/system, and <threads.h>

```{task} Add Base signals, waitid, _Fork, popen/system, and <threads.h>
:id: TSK_0034
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0031, SWR_PROC_0012

Add `_Fork`, `waitid`, `execl`, `execle`, `execlp`, `fexecve`, `popen`,
`pclose`, and `system`. Add `pause`, `sigsuspend`, `sigwait`, `sigwaitinfo`,
`sigtimedwait`, `sigqueue`, `sig2str`, `str2sig`, `strsignal`, `psignal`, and
`psiginfo`. Add C11 `<threads.h>`, remaining Base `pthread_*` and `sched_*`
helpers, and named `sem_open`/`sem_close`/`sem_unlink`. Add `sleep`, `time`,
`timeout`, `nohup`, and `sh` `wait` ({need}`BDR_0004`, {need}`SWR_PROC_0012`,
{need}`SYS_PROC_0001`, {need}`SYS_SIG_0001`).
```

### TSK_0033 — Add VFS links, permissions, FIFOs, and <sys/statvfs.h>

```{task} Add VFS links, permissions, FIFOs, and <sys/statvfs.h>
:id: TSK_0033
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0030, SWR_IPC_0012

Add `chmod*`, `chown*`, `faccessat`, `link*`, `symlink*`, `readlink*`, and
`mkfifo*` in `vfsserv` and `pipeserv`. Add `pread`, `pwrite`, `truncate`,
`posix_close`, `posix_getdents`, `scandir`, `alphasort`, `readdir_r`,
`<sys/statvfs.h>`, `remove`, and temporary-file helpers. Add file utilities
`chmod`, `chown`, `chgrp`, `ln`, `mkfifo`, `readlink`, `realpath`, and `touch`.
Also add `rmdir`, `df`, `du`, `dd`, `basename`, `dirname`, and `pathchk`
({need}`BDR_0004`, {need}`SWR_IPC_0012`).
```

### TSK_0032 — Add credentials, <pwd.h>, <grp.h>, and <sys/resource.h>

```{task} Add credentials, <pwd.h>, <grp.h>, and <sys/resource.h>
:id: TSK_0032
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0029, SWR_PROC_0011

Add process user, group, supplementary group, `nice`, and `rlimit` fields to
`pcb_t` with kernel syscalls. Add `<pwd.h>`, `<grp.h>`, `<sys/resource.h>`
(`getrlimit`, `setrlimit`), `getuid`/`setuid`/`geteuid`/`seteuid`,
`getgid`/`setgid`/`getegid`/`setegid`, `getgroups`, and `getlogin`/`getlogin_r`.
Add `id`, `logname`, `nice`, `renice`, and `sh` built-in commands `umask` and
`ulimit` ({need}`BDR_0004`, {need}`SWR_PROC_0011`).
```

### TSK_0031 — Add POSIX.1-2024 ps, sysconf/confstr/pathconf, and getconf

```{task} Add POSIX.1-2024 ps, sysconf/confstr/pathconf, and getconf
:id: TSK_0031
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0028, SWR_PROC_0010

Add kernel `pcb_t` command metadata and read-only `FD_OP_PROC_LIST` snapshots
for Ring 3 `ps`. Add Base `<unistd.h>` `sysconf`, `confstr`, `pathconf`,
`fpathconf`, `gethostname`, `ctermid`, `ttyname`, `ttyname_r`, `tcgetsid`,
`tcgetwinsize`, and `tcsetwinsize` with no non-POSIX `_SC_*` names. Add `ps`,
`getconf`, `tty`, and `stty`. Change Gate 2 to count standard header macros
({need}`BDR_0004`, {need}`SWR_PROC_0010`).
```

### TSK_0025 — Boot secondary CPUs with ACPI MADT and add SMP spinlocks

```{task} Boot secondary CPUs with ACPI MADT and add SMP spinlocks
:id: TSK_0025
:status: done
:traces: ADR_0003, ADR_0022, ADR_0024, SWR_PROC_0009

Start secondary CPUs with `-smp 2` behind portable HAL interfaces (`INIT`/`SIPI`
and `GS.base` on `x86-64`). Lock `pmm`, `vmm`, `sched`, `ipc`, and `futex` data
in concurrent multi-core QEMU tests ({need}`SWR_PROC_0009`).
```

### TSK_0028 — Mount writable on-disk filesystem in vfsserv on /dev/blk

```{task} Mount writable on-disk filesystem in vfsserv on /dev/blk
:id: TSK_0028
:status: done
:traces: ADR_0003, ADR_0016, SWR_IPC_0011

Mount a writable on-disk filesystem (such as `ext2`) in `vfsserv` on `/dev/blk`.
Make sure that files and directories stay across QEMU boots
({need}`SWR_IPC_0011`).
```

### TSK_0024 — Add MMIO/DMA syscalls and Ring 3 PCI virtio-blk driver

```{task} Add MMIO/DMA syscalls and Ring 3 PCI virtio-blk driver
:id: TSK_0024
:status: done
:traces: ADR_0003, ADR_0008, ADR_0016, SWR_IPC_0010

Add `CAP_SYS_INTR` MMIO device maps, PCI configuration, and DMA buffer syscalls.
Make a Ring 3 PCI `virtio-blk` driver (`/dev/blk`) with IOAPIC IRQ pulses. Do
tests of sector persistence across QEMU boots ({need}`SWR_IPC_0010`).
```

### TSK_0030 — Add POSIX.1-2024 <math.h> and <fenv.h> with AVX2/FMA

```{task} Add POSIX.1-2024 <math.h> and <fenv.h> with AVX2/FMA
:id: TSK_0030
:status: done
:traces: ADR_0003, ADR_0026, SWR_LIBC_0009

Add `<fenv.h>` (`fegetround`, `fesetround`, float flags) and `<math.h>` (`sqrt`,
`fma`, float modes, IEEE-754 class macros, `exp`/`log`/`pow`, and trigonometric
functions such as `sin`/`cos`/`tan`/`asin`/`acos`/`atan`) with `x86-64-v3`
hardware `FMA`. Do tests in Gates 1 to 3 and QEMU ({need}`SWR_LIBC_0009`).
```

### TSK_0029 — Add x86-64-v3 boot gate, XSAVE/XRSTOR, and optional AVX-512

```{task} Add x86-64-v3 boot gate, XSAVE/XRSTOR, and optional AVX-512
:id: TSK_0029
:status: done
:traces: ADR_0024, ADR_0026, SWR_BOOT_0005

Add an initial `CPUID` check for `x86-64-v3` (`XSAVE`, `AVX`, `AVX2`, `FMA`,
`BMI1`/`BMI2`) that stops older CPUs on boot. Change `switch.S` to
`XSAVE`/`XRSTOR` with optional `AVX-512` `XCR0` checks. Compile Ring 3 with
`-march=x86-64-v3`. Add positive and negative QEMU tests
({need}`SWR_BOOT_0005`).
```

### TSK_0022 — Add TTY job control, ISIG (Ctrl-C), process groups, and utilities

```{task} Add TTY job control, ISIG (Ctrl-C), process groups, and utilities
:id: TSK_0022
:status: done
:traces: ADR_0003, ADR_0018, ADR_0021, SWR_PROC_0008

Add `ISIG` (`VINTR` to `SIGINT`) in `ttyserv`, process groups (`setpgid`,
`getpgid`, `setsid`, `tcgetpgrp`, `tcsetpgrp`), `waitpid(0)`/`-pgid`, and
`kill(-pgid)`. Add Ring 3 utilities (`mkdir`, `rm`, `cp`, `mv`, `uname`, `env`,
`wc`, `head`) for the interactive shell ({need}`SWR_PROC_0008`).
```

### TSK_0023 — Add primary libc Base headers and standard C utilities

```{task} Add primary libc Base headers and standard C utilities
:id: TSK_0023
:status: done
:traces: ADR_0003, WDR_0002, SWR_LIBC_0008

Record compiler freestanding headers in Gate 2 and add `<sys/utsname.h>`,
`<endian.h>`, `<sched.h>`, `<setjmp.h>`, `<sys/times.h>`, and `<fnmatch.h>`. Add
`atexit`, `qsort`, `bsearch`, `strtol`, `strtoul`, `getopt`, `alarm`, `rename`,
and `realpath` ({need}`SWR_LIBC_0008`).
```

### TSK_0027 — Add optional automatic LLVM code coverage pipeline

```{task} Add optional automatic LLVM code coverage pipeline
:id: TSK_0027
:status: done
:traces: ADR_0025, SWR_DOCS_0005

Add `-DMICROKERNEL_COVERAGE=ON` LLVM instrumentation, linker `__llvm_prf_*`
symbols, and a freestanding Raw Profile v8 writer. Send base64 records through
`arch_console_write()` with `SYS_COV_WRITE` and `test_runner.py --cov-out`. Add
`tools/coverage/generate_report.py` and `./scripts/env.sh coverage` for HTML,
LCOV, and text reports in `build-cov/` ({need}`SWR_DOCS_0005`).
```

### TSK_0026 — Isolate x86-64 platform code behind x86-64 HAL directories

```{task} Isolate x86-64 platform code behind x86-64 HAL directories
:id: TSK_0026
:status: done
:traces: ADR_0024, SWR_BOOT_0004

Move `x86-64` kernel, `libc`, and UART/RTC platform code into `x86-64`
directories behind a portable HAL. Configure CMake, QEMU, and test scripts with
`-DMICROKERNEL_ARCH=x86-64`. Keep `x86-64` as the only active target
({need}`SWR_BOOT_0004`).
```

### TSK_0020 — Add pselect and multi-subsystem leak checks

```{task} Add pselect and multi-subsystem leak checks
:id: TSK_0020
:status: done
:traces: ADR_0003, WDR_0002, SWR_DOCS_0004

Add `<sys/select.h>` (`select`, `pselect`) through multi-server `ppoll()` with
Gate 1 to Gate 3 checks. Add kernel frame count and server object count checks.
Make sure that no leaks occur across `sh`, VFS, pipes, TTY, `AF_UNIX` sockets,
`SHM`, signals, and pthreads ({need}`SWR_DOCS_0004`).
```

### TSK_0021 — Add ACPI MADT/SPCR discovery and parameterized Ring 3 drivers

```{task} Add ACPI MADT/SPCR discovery and parameterized Ring 3 drivers
:id: TSK_0021
:status: done
:traces: ADR_0003, SWR_BOOT_0003, SWR_IPC_0008

Parse ACPI `MADT` IOAPIC bases, Interrupt Source Overrides (ISOs), and `SPCR` if
available. If `SPCR` is missing, use COM1 `0x3F8`/IRQ 4 as an alternative. Give
discovered `argv` (`-p`/`-i`) to `ttyserv` and `rtcdrv`. Send user
`stdout`/`stderr` TX (`OPOST`/`ONLCR`) through `ttyserv` with a safe
initial-boot alternative ({need}`SWR_BOOT_0003`).
```

### TSK_0017 — Make Ring 3 command shell (sh) and primary utilities

```{task} Make Ring 3 command shell (sh) and primary utilities
:id: TSK_0017
:status: done
:traces: ADR_0003, SWR_LIBC_0006

Add Ring 3 command shell (`sh`) and small primary utilities (`echo`, `cat`,
`ls`, `true`, `false`, `test`) for POSIX.1-2024 XCU. Include pipelines, file
redirections, `$'...'` syntax, `set -o pipefail`, and `read -d`
({need}`SWR_LIBC_0006`).
```

### TSK_0018 — Make Ring 3 AF_UNIX local sockets server and accept4

```{task} Make Ring 3 AF_UNIX local sockets server and accept4
:id: TSK_0018
:status: done
:traces: ADR_0003, SWR_IPC_0009

Add Ring 3 `sockserv` (`/dev/sock`), `<sys/socket.h>`, and `<sys/un.h>`
(`socket`, `socketpair`, `bind`, `listen`, `accept`, `accept4` with
`SOCK_CLOEXEC` and `SOCK_NONBLOCK`, `connect`, `send`, `recv`) with multi-server
`ppoll()` ({need}`SWR_IPC_0009`).
```

### TSK_0019 — Add POSIX buffered stdio, env, execvp, and primary C17 headers

```{task} Add POSIX buffered stdio, env, execvp, and primary C17 headers
:id: TSK_0019
:status: done
:traces: ADR_0003, SWR_LIBC_0007

Add `<ctype.h>`, `<assert.h>`, `<inttypes.h>`, `<strings.h>`, and `<limits.h>`
POSIX limits. Add buffered `FILE` I/O (`fopen`, `fdopen`, `fclose`, `fread`,
`fwrite`, `fgets`, `fseek`, `fileno`), `setenv`, `unsetenv`, `execv`, `execvp`,
`strdup`, `strstr`, and `strerror` ({need}`SWR_LIBC_0007`).
```

### TSK_0016 — Make Ring 3 serial TTY server, termios, and multi-server ppoll

```{task} Make Ring 3 serial TTY server, termios, and multi-server ppoll
:id: TSK_0016
:status: done
:traces: ADR_0003, SWR_IPC_0008

Add Ring 3 `ttyserv` (`/dev/tty`) with COM1 IRQ 4 pulses, `<termios.h>`,
`isatty`, and synchronous `stdin` `read()`. Add multi-server `ppoll()` dispatch
across pipes, VFS, and TTY descriptors ({need}`SWR_IPC_0008`).
```

### TSK_0012 — Add option-gated POSIX memory extensions (SHM, ML, MLR)

```{task} Add option-gated POSIX memory extensions (SHM, ML, MLR)
:id: TSK_0012
:status: done
:traces: ADR_0003, SWR_MEM_0005

Add kernel shared-frame `MAP_SHARED` memory for `shm_open`/`shm_unlink` (`SHM`).
Add `mlock`, `munlock`, `mlockall`, and `munlockall` (`ML`/`MLR`) behind
standard `_POSIX_*` macros with QEMU and Gate 2 tests ({need}`SWR_MEM_0005`).
```

### TSK_0015 — Make Ring 3 VFS server with bootfs, tmpfs, stat, and dirent

```{task} Make Ring 3 VFS server with bootfs, tmpfs, stat, and dirent
:id: TSK_0015
:status: done
:traces: ADR_0003, SWR_IPC_0007

Add Ring 3 `vfsserv` (`/dev/vfs`) that gives read-only `bootfs` and writable
`tmpfs`. Add `<sys/stat.h>`, `<dirent.h>`, `open`/`openat`, `lseek`,
`ftruncate`, `stat`/`fstat`, `mkdir`, `unlink`, `chdir`, and `getcwd`
({need}`SWR_IPC_0007`).
```

### TSK_0014 — Add mmap-backed thread-safe malloc and free allocator

```{task} Add mmap-backed thread-safe malloc and free allocator
:id: TSK_0014
:status: done
:traces: ADR_0003, SWR_MEM_0006

Replace the static 1 MiB linear arena in `libc/src/stdlib.c` with a thread-safe
`mmap`/`munmap` slab allocator. Release memory on `free`, `realloc`, and
`reallocarray` ({need}`SWR_MEM_0006`).
```

### TSK_0013 — Add full 4-gate POSIX.1-2024 compatibility harness

```{task} Add full 4-gate POSIX.1-2024 compatibility harness
:id: TSK_0013
:status: done
:traces: WDR_0002, SWR_DOCS_0003

Connect automatic `issue8-manifest.tsv` symbol diff reports and cancellation
call graph checks into CTest with `posix.headers` and `posix.async_signal_safe`
({need}`SWR_DOCS_0003`).
```

### TSK_0011 — Add POSIX.1-2024 Base target functions and C11/C17 headers

```{task} Add POSIX.1-2024 Base target functions and C11/C17 headers
:id: TSK_0011
:status: done
:traces: ADR_0003, ADR_0014, SWR_LIBC_0005

Add `strlcpy`, `strlcat`, `reallocarray`, `getentropy`, and `secure_getenv` with
`-D_POSIX_C_SOURCE=202405L`, plus `<stdatomic.h>`, `<stdalign.h>`, and
`<uchar.h>` with QEMU and CTest tests ({need}`SWR_LIBC_0005`).
```

### TSK_0010 — Add pthread cancellation points and thread-safe <stdio.h> mutexes

```{task} Add pthread cancellation points and thread-safe <stdio.h> mutexes
:id: TSK_0010
:status: done
:traces: ADR_0003, ADR_0013, SWR_LIBC_0004

Add `pthread_cancel`, `pthread_setcancelstate`, `pthread_setcanceltype`,
`pthread_testcancel`, and `<stdio.h>` stream lock functions (`flockfile`,
`funlockfile`) with QEMU and CTest tests ({need}`SWR_LIBC_0004`).
```

### TSK_0009 — Add POSIX.1-2024 Base target asynchronous I/O (<aio.h>)

```{task} Add POSIX.1-2024 Base target asynchronous I/O (<aio.h>)
:id: TSK_0009
:status: done
:traces: ADR_0003, ADR_0012, SWR_IPC_0005

Add `<aio.h>` (`aio_read`, `aio_write`, `aio_error`, `aio_return`, `aio_cancel`,
`aio_fsync`, `aio_suspend`, `lio_listio`) with Ring 3 worker threads or servers.
Keep `<mqueue.h>` behind `_POSIX_MESSAGE_PASSING` ({need}`SWR_IPC_0005`).
```

### TSK_0007 — Add POSIX.1-2024 target fork, execve, and pipe2/dup3/ppoll

```{task} Add POSIX.1-2024 target fork, execve, and pipe2/dup3/ppoll
:id: TSK_0007
:status: done
:traces: ADR_0003, ADR_0011, SWR_PROC_0004, SWR_IPC_0004

Make `fork` on full-address-space `VMCloneCow` and `execve` from `boot.tar`. Add
process file descriptors and `pipe2`/`dup3`/`ppoll` with QEMU and CTest tests
({need}`SWR_PROC_0004`, {need}`SWR_IPC_0004`).
```

### TSK_0008 — Move older scaffold documents into Sphinx-Needs

```{task} Move older scaffold documents into Sphinx-Needs
:id: TSK_0008
:status: done
:traces: WDR_0009, ADR_0001, CNV_0005, CNV_0006

Move all unique milestones, decisions (`ADR-1` through `ADR-19`), conventions,
vision rules, and context from the five older documents into Sphinx-Needs. Point
governance and skills to the new records, keep `docs/THIRD-PARTY-NOTICES.md`,
and remove the five older files ({need}`WDR_0009`).
```

### TSK_0006 — Add POSIX.1-2024 target pthreads, semaphores, and FS.base TLS

```{task} Add POSIX.1-2024 target pthreads, semaphores, and FS.base TLS
:id: TSK_0006
:status: done
:traces: ADR_0003, ADR_0010, SWR_PROC_0005

Add thread-local `FS.base` TLS, `<pthread.h>` (`pthread_create`, `pthread_join`,
mutexes, `pthread_cond_*`, `pthread_cond_clockwait`), and `<semaphore.h>`
(`sem_*`) on kernel wait queues ({need}`SWR_PROC_0005`).
```

### TSK_0005 — Add POSIX.1-2024 target monotonic clocks and timers

```{task} Add POSIX.1-2024 target monotonic clocks and timers
:id: TSK_0005
:status: done
:traces: ADR_0003, ADR_0009, SWR_PROC_0006

Add kernel monotonic and realtime clock syscalls and `<time.h>`
(`clock_gettime`, `clock_settime`, `clock_nanosleep`, `timer_create`,
`timer_settime`) for `CLOCK_MONOTONIC` and `CLOCK_REALTIME`
({need}`SWR_PROC_0006`).
```

### RDM_0002 — Milestone 6: Interrupts and user-space drivers

```{roadmap} Milestone 6: Interrupts and user-space drivers
:id: RDM_0002
:status: done
:traces: SYS_IPC_0001, SWR_IPC_0004, SWR_IPC_0005, SWR_IPC_0006, ADR_0008

This milestone sends hardware interrupt events as IPC messages
(`AttachIntrEvent`). It adds `IOPLSet` and multi-buffer `ReadMsgV`/`WriteMsgV`
and operates a Ring 3 QEMU device driver ({need}`SYS_IPC_0001`).
```

### TSK_0004 — Add Milestone 6 interrupts, port I/O, and Ring 3 driver

```{task} Add Milestone 6 interrupts, port I/O, and Ring 3 driver
:id: TSK_0004
:status: done
:traces: SYS_IPC_0001, SWR_IPC_0004, SWR_IPC_0006, ADR_0008

Add `AttachIntrEvent`, `IntrUnmask`, `IOPLSet`, and multi-buffer
`ReadMsgV`/`WriteMsgV`. Do tests of a Ring 3 QEMU device driver in headless QEMU
({need}`SYS_IPC_0001`, {need}`SWR_IPC_0004`).
```

### RDM_0001 — Milestone 5: Processes and memory syscalls

```{roadmap} Milestone 5: Processes and memory syscalls
:id: RDM_0001
:status: done
:traces: SYS_PROC_0001, SYS_MEM_0001, SWR_MEM_0004, SWR_PROC_0004, ADR_0007

This milestone adds user-space process creation (`ProcCreate`), virtual memory
maps (`VMMap`, `VMUnmap`), copy-on-write clones (`VMCloneCow`), and parent
exit-code collection ({need}`SYS_PROC_0001`, {need}`SYS_MEM_0001`).
```

### TSK_0002 — Add Milestone 5 process and memory syscalls

```{task} Add Milestone 5 process and memory syscalls
:id: TSK_0002
:status: done
:traces: SYS_PROC_0001, SYS_MEM_0001, SWR_MEM_0004, SWR_PROC_0004, ADR_0007

Add `ProcCreate`, `VMMap`, `VMUnmap`, `VMCloneCow`, child exit-code reports, and
automatic QEMU tests to prepare for POSIX `mmap` and `waitpid`
({need}`SWR_MEM_0004`, {need}`SWR_PROC_0004`).
```

### TSK_0003 — Align signal ABI, masks, and SIGKILL/SIGSTOP rules

```{task} Align signal ABI, masks, and SIGKILL/SIGSTOP rules
:id: TSK_0003
:status: done
:traces: ADR_0003, ADR_0006, SWR_SIG_0003, SWR_SIG_0004

Add `<signal.h>` with POSIX `sigaction` and `kill`, and reject `SIGKILL` and
`SIGSTOP` in `sys_sigaction`. Keep `blocked_signals` in `sig_frame_t` and do a
check of async-signal-safety ({need}`SWR_SIG_0003`, {need}`SWR_SIG_0004`).
```

### TSK_0001 — Align libc headers and thread-local errno to POSIX.1-2024

```{task} Align libc headers and thread-local errno to POSIX.1-2024
---
id: TSK_0001
status: done
traces: >
  BDR_0003, ADR_0003, ADR_0005, SWR_LIBC_0002, SWR_LIBC_0003, SWR_DOCS_0003
---

Remove `<sys/microkernel.h>` leakage from `<unistd.h>` and set `_POSIX_VERSION`
and `_POSIX2_VERSION` to `202405L`. Add `<errno.h>` with thread-local `errno`
and the standalone header gate ({need}`SWR_LIBC_0002`, {need}`SWR_LIBC_0003`,
{need}`SWR_DOCS_0003`).
```

### RDM_0004 — Milestone 4: Threads and signals

```{roadmap} Milestone 4: Threads and signals
:id: RDM_0004
:status: done
:traces: SYS_PROC_0001, SYS_SIG_0001

This milestone adds `NewThread`, `ThreadReset`, `ThreadGetCtx`, and
kernel-allocated user stacks. It sends Ring 3 signal frames, `kill` with `EINTR`
wakeup, and default process stop, and `sigtest` does tests of these interfaces
({need}`SYS_PROC_0001`, {need}`SYS_SIG_0001`).
```

### RDM_0005 — Milestone 3: Messages between processes

```{roadmap} Milestone 3: Messages between processes
:id: RDM_0005
:status: done
:traces: SYS_IPC_0001

This milestone adds priority-ordered channels, `SendMsg`, `ReceiveMsg`,
`ReplyMsg`, and priority inheritance. It adds connections, permission checks on
copies between address spaces, the `sysproc` name registry, and
`ipcserv`/`ipccli` echo tests ({need}`SYS_IPC_0001`).
```

### RDM_0006 — Milestone 2: User processes

```{roadmap} Milestone 2: User processes
:id: RDM_0006
:status: done
:traces: SYS_PROC_0001, SYS_LIBC_0001

This milestone adds the four-step build, small freestanding `libc`, and
`syscall`/`sysret` dispatch. It adds the preemptive scheduler, ELF loader, and
Ring 3 `sysproc` loaded from `boot.tar` ({need}`SYS_PROC_0001`,
{need}`SYS_LIBC_0001`).
```

### RDM_0007 — Milestone 1: Boot and memory

```{roadmap} Milestone 1: Boot and memory
:id: RDM_0007
:status: done
:traces: SYS_BOOT_0001, SYS_MEM_0001

This milestone adds the Podman Clang/CMake build container, GRUB Multiboot2
higher-half boot, and the bitmap physical memory manager. It adds the W^X
virtual memory manager, GDT, IDT, PIT-calibrated LAPIC timer, and QEMU test
harness ({need}`SYS_BOOT_0001`, {need}`SYS_MEM_0001`).
```
