## Purpose

Record the important changes of microkernel. The format follows [Keep a
Changelog](https://keepachangelog.com).

## Structure

- The `## [Unreleased]` section is at the top. Below it, the releases are newest
  first (git log order).
- Category headings of level 3 in each section: `### Added`, `### Changed`, `###
  Deprecated`, `### Removed`, `### Fixed`, and `### Security`. Put a heading
  into the file only when the section has content for it.

## Formatting Rules

- Release heading: `## [x.y.z] - YYYY-MM-DD`.
- The entries in each category are newest first (git log order).
- Write each entry as a complete sentence with a final period.

## [Unreleased]

### Fixed

- Fixed `tests/stresstest/test_stress.c` boot-quiescence threshold from 8 to 9
  live processes (8 persistent servers including `blkdrv` plus `stresstest`) and
  added post-stress SMP zombie-stack settle polling so `stresstest` waits
  accurately for earlier boot tests in instrumented coverage builds without
  false PMM leak mismatches.
- Reclassified six mislabeled option-group entries in
  `tests/posix/issue8-manifest.tsv` (`encrypt` and `setkey` from `BASE` to
  `XSI`; `posix_spawnattr_{get,set}sched{param,policy}` from `BASE` to `SPN`) so
  Gate 2 (`posix.symbol_diff`) reports exact POSIX.1-2024 Base interface counts
  (`751/1041`).
- Serialized LLVM coverage framing (`cov_dump_proc()`, `cov_dump_kernel()`, and
  `@@COV_COMPLETE@@`) in `kernel/core/cov.c` under `g_console_lock` via
  `console_lock_irqsave()` / `console_unlock_irqrestore()` so SMP `printk()` and
  `ttyserv` `FD_OP_CONSOLE_TX` output never interleaves into base64 `.profraw`
  frames or swallows `[OK]` test expectations during `./scripts/env.sh
  coverage`.
- Added retry polling to `/dev/echo` name resolution in `tests/ipccli/main.c` so
  SMP boot scheduling order never causes spurious `NAME_MSG_QUERY` failures.

### Changed

- Archived completed `RDM_0003` (`70/70` Base headers, `1037/1037` Base
  interfaces, `108/108` Base XCU utilities) to `docs/records/HISTORY.md`, added
  `RDM_0009` to `docs/records/ROADMAP.md`, and queued six dynamic-linking
  milestones (`TSK_0061`–`TSK_0066`) in `docs/records/TASKS.md` covering kernel
  `PT_INTERP`/SysV `auxv`, Ring 3 `ld.so`, shared `libc.so`/`libz.so.1`/OpenSSL,
  dynamically linked OpenSSH Portable, and dynamically linked `bin/` utilities.
- Enabled Ring 3 sysroot OpenSSL `3.0.15` (`libcrypto.a`) and `zlib 1.3.1`
  (`libz.a`) in OpenSSH Portable `9.9p2` (`cmake/openssh.cmake`,
  `cmake/openssh_config.h`, `TSK_0060`), added Ed25519, ECDSA P-256, and
  RSA-2048 host key generation plus `Compression delayed` in
  `bin/utils/sshd_setup.c`, and expanded CTest `ipc.sshd_e2e` to verify RSA
  (`aes128-gcm@openssh.com`), ECDSA P-256 (`ecdh-sha2-nistp256`), Ed25519, and
  `zlib@openssh.com` compressed SSH sessions.
- Switched optional technical writing checks in `tools/check_ste.py`,
  `scripts/build-agent.sh`, and `scripts/install-hooks.sh` to
  `ste-writing-skill` (`STE_WRITING_SKILL`) with clean skip behavior when
  absent, and simplified documentation rules across `AGENTS.md`, `README.md`,
  `CNV_0002`, and WDRs to reference the `ste-writing-skill` skill.
- Queued three incremental milestones in `docs/records/TASKS.md` for Ring 3
  OpenSSH Portable (`TSK_0055`: host-to-guest networking, `netserv` hardening,
  `nc`, and `ping`; `TSK_0056`: Ring 3 PTYs, `/dev/null`, `/dev/urandom`,
  `SCM_RIGHTS`, and OpenSSH `libc` APIs; `TSK_0057`: OpenSSH Portable `sshd` and
  `ssh-keygen`).
- Split `pax` out of `TSK_0046` into `TSK_0054` in `docs/records/TASKS.md` so
  file comparison/patching (`diff`, `cmp`, `comm`, `patch`) and archive
  interchange (`pax`) land as focused milestones.
- Reordered `docs/records/TASKS.md` to pull Ring 3 TCP/IP networking
  (`TSK_0041`) and the five remaining network Base headers (`TSK_0047`) ahead of
  the remaining XCU utility waves, and split `TSK_0048` into `TSK_0048`
  (encoding, logging, terminal, and user utilities) and `TSK_0053` (scheduling,
  spooling, and mail utilities).
- Reordered and split the remaining POSIX.1-2024 Base task backlog in
  `docs/records/TASKS.md`: moved i18n headers (`TSK_0038`) and `<dlfcn.h>`
  (`TSK_0040`) ahead of the XCU utility waves so all non-network Base C headers
  finish first, and split large utility waves into `TSK_0043`/`TSK_0050`,
  `TSK_0046`/`TSK_0051`, and `TSK_0044`/`TSK_0052`.
- Synced `tests/posix/issue8-manifest.tsv` with the updated
  `posix-issue8-os-dev` manifest, activated and header-gated the implemented
  option groups (`PS`, `SIO`, `TSA`, `RPP`) in `libc/include/sys/_posix_opts.h`,
  `aio.h`, `sys/mman.h`, `sched.h`, `pthread.h`, `sysconf.c`, and Gate 2
  (`ALT_OPTION_GROUPS` for `FSC|SIO` and `XSI|SIO`), and queued `TSK_0049` in
  `docs/records/TASKS.md` for the seven newly-Base interfaces (`futimens`,
  `utimensat`, `lgamma*`, `tzset`, `tzname`).
- Refined, split, and reordered the remaining POSIX.1-2024 Base task backlog in
  `docs/records/TASKS.md`: explicitly listed `<stdlib.h>` multibyte, `*_l`
  locale helpers, and `wcstoimax`/`wcstoumax` in `TSK_0037`; split wide `stdio`
  (`TSK_0045`), diff/archive utilities (`TSK_0046`), network Base headers
  (`TSK_0047`), and remaining batch/spool utilities (`TSK_0048`); and moved
  `<regex.h>`/`<glob.h>` (`TSK_0039`), shell grammar (`TSK_0042`), and text
  utilities (`TSK_0043`, `TSK_0046`) ahead of i18n catalogs (`TSK_0038`) and
  networking.
- Rewrote the `User-space functionality (current support)` section of
  `README.md` from a shell-user, C17-programmer, and Ring 3 server/driver
  perspective instead of an exhaustive symbol dump, linking to
  `build/posix-symbol-diff.txt` for the full machine-generated inventory.

### Added

- Added CMake `FetchContent` cross-compilation of OpenSSL `3.0.15` LTS into the
  Ring 3 sysroot (`sysroot/usr/lib/libcrypto.a`, `sysroot/usr/lib/libssl.a`,
  `<openssl/*.h>`, downloaded at build time with zero vendored source) with QEMU
  verification in `tests/openssltest/test_openssl.c` (SHA-256, AES-256-GCM,
  RSA-2048, ECDSA P-256, Ed25519, X.509, and TLS 1.3; `TSK_0059`,
  `SWR_LIBC_0031`, `ADR_0056`, `COMP_LIBC_0030`, `VER_LIBC_0031`).
- Added CMake `FetchContent` cross-compilation of `zlib` `1.3.1` into the Ring 3
  sysroot (`sysroot/usr/lib/libz.a`, `<zlib.h>`, `<zconf.h>`, downloaded at
  build time with zero vendored source) with QEMU verification in
  `tests/zlibtest/test_zlib.c` (`TSK_0058`, `SWR_LIBC_0030`, `ADR_0055`,
  `COMP_LIBC_0029`, `VER_LIBC_0030`).
- Added CMake `FetchContent` cross-compilation of OpenSSH Portable `9.9p2`
  (`/boot/sshd`, `/boot/sshd-session`, `/boot/ssh-keygen`, downloaded at build
  time with zero vendored source), `/boot/sshd-setup` auto-start on `init=sh`
  interactive boot (Ed25519 host key, `/mnt/etc/passwd` with dev password
  `microkernel`, `/mnt/etc/ssh/sshd_config`), and real OpenSSH client CTest
  `ipc.sshd_e2e` verifying Ed25519 public-key exec, password login, and
  interactive `-tt` PTY sessions (`TSK_0057`, `SWR_IPC_0016`, `ADR_0054`,
  `COMP_IPC_0021`, `VER_IPC_0014`).
- Added Ring 3 PTYs (`/dev/ptmx`, `/dev/pts/N`, `posix_openpt`/`grantpt`/
  `unlockpt`/`ptsname*`, `<sys/ioctl.h>`, `SIGWINCH`), virtual character devices
  (`/dev/null`, `/dev/zero`, ChaCha20 `/dev/urandom`), Ring 3 `AF_UNIX`
  `SCM_RIGHTS` FD passing, and XSI/OpenSSH `libc` APIs (`<syslog.h>`,
  `<sys/time.h>`, `<sys/uio.h>`, `crypt`, `getrusage`, `pw`/`gr` iterators,
  `setresuid`/`setresgid`, `initgroups`, `daemon`, `chroot`) with `SIGWINCH`
  default-ignore disposition and zero new Ring 0 syscalls (`TSK_0056`,
  `SWR_IPC_0015`, `SWR_LIBC_0029`, `ADR_0053`, `ICD_IPC_0018`, `ICD_LIBC_0018`,
  `COMP_IPC_0020`, `COMP_LIBC_0028`, `VER_IPC_0013`, `VER_LIBC_0029`).
- Added localhost-only host-to-guest TCP port forwarding in `scripts/env.sh run`
  (`2222:22,8080:8080`), hardened `servers/netserv/` (32 KiB TCP RX buffers,
  window-update ACKs, passive ARP learning, duplicate-`SYN` replies, and TX
  virtqueue backpressure), added Ring 3 `nc` and `ping` utilities, and added
  CTest `ipc.net_hostfwd` (`TSK_0055`, `SWR_IPC_0014`, `ADR_0052`,
  `COMP_IPC_0019`, `VER_IPC_0012`).
- Added POSIX.1-2024 XCU `at`, `batch`, `crontab`, `lp`, and `mailx` in Ring 3
  `bin/utils/` (`TSK_0053`, `SWR_LIBC_0028`, `ADR_0051`, `COMP_LIBC_0027`,
  `VER_LIBC_0028`), reaching 108/108 Base XCU utilities (alongside 70/70 Base C
  headers and 1037/1037 Base C interfaces) with QEMU tests in
  `tests/shtest/test_sh.c`.
- Added POSIX.1-2024 XCU `uuencode`, `uudecode`, `logger`, `tput`, `tabs`,
  `mesg`, `write`, and `newgrp` in Ring 3 `bin/utils/` (`TSK_0048`,
  `SWR_LIBC_0027`, `ADR_0050`, `COMP_LIBC_0026`, `VER_LIBC_0027`), advancing
  Gate 2 from 95/108 to 103/108 Base XCU utilities with QEMU tests in
  `tests/shtest/test_sh.c`.
- Added POSIX.1-2024 XCU `bc` (`-l`) in Ring 3 `bin/bc/` and `m4` in Ring 3
  `bin/utils/m4.c` (`TSK_0052`, `SWR_LIBC_0026`, `ADR_0049`, `COMP_LIBC_0025`,
  `VER_LIBC_0026`), advancing Gate 2 from 93/108 to 95/108 Base XCU utilities
  with QEMU tests in `tests/shtest/test_sh.c`.
- Added POSIX.1-2024 XCU `awk` in Ring 3 `bin/awk/` and `ed` in Ring 3
  `bin/utils/ed.c` (`TSK_0044`, `SWR_LIBC_0025`, `ADR_0048`, `COMP_LIBC_0024`,
  `VER_LIBC_0025`) over `<regex.h>` and `<math.h>`, advancing Gate 2 from 91/108
  to 93/108 Base XCU utilities with QEMU tests in `tests/shtest/test_sh.c`.
- Added POSIX.1-2024 XCU `split`, `csplit`, `fold`, `expand`, `unexpand`,
  `strings`, `tsort`, `file`, and `pr` in Ring 3 `bin/utils/` (`TSK_0051`,
  `SWR_LIBC_0024`, `ADR_0047`, `COMP_LIBC_0023`, `VER_LIBC_0024`), advancing
  Gate 2 from 82/108 to 91/108 Base XCU utilities with QEMU tests in
  `tests/shtest/test_sh.c`.
- Added POSIX.1-2024 XCU `pax` in Ring 3 `bin/utils/pax.c` (`TSK_0054`,
  `SWR_LIBC_0023`, `ADR_0046`, `COMP_LIBC_0022`, `VER_LIBC_0023`) supporting
  list, extract (`-r`), write (`-w`), and copy-pass (`-rw`) modes over `<tar.h>`
  `ustar` and `<cpio.h>` ODC formats with `-s` regex path rewriting, advancing
  Gate 2 from 81/108 to 82/108 Base XCU utilities.
- Added POSIX.1-2024 XCU `diff`, `cmp`, `comm`, and `patch` in Ring 3
  `bin/utils/` (`TSK_0046`, `SWR_LIBC_0022`, `ADR_0045`, `COMP_LIBC_0021`,
  `VER_LIBC_0022`), advancing Gate 2 from 77/108 to 81/108 Base XCU utilities
  with unified/normal/context/reverse round-trip tests in
  `tests/shtest/test_sh.c`.
- Added POSIX.1-2024 XCU `sort`, `uniq`, `cut`, `tr`, `tail`, `tee`, `paste`,
  `join`, and `cksum` in Ring 3 `bin/utils/` (`TSK_0050`, `SWR_LIBC_0021`,
  `ADR_0044`, `COMP_LIBC_0020`, `VER_LIBC_0021`), advancing Gate 2 from 68/108
  to 77/108 Base XCU utilities with QEMU pipeline tests in
  `tests/shtest/test_sh.c`.
- Added the remaining five POSIX.1-2024 Base network headers (`<netinet/in.h>`,
  `<netinet/tcp.h>`, `<arpa/inet.h>`, `<net/if.h>`, and `<netdb.h>`) and wired
  `AF_INET`/`AF_INET6` sockets, `sendmsg`, `recvmsg`, `sockatmark`, and
  multi-server `ppoll`/`select` over `/dev/net` (`TSK_0047`, `SWR_LIBC_0020`,
  `ADR_0043`, `ICD_LIBC_0017`, `COMP_LIBC_0019`, `VER_LIBC_0020`), reaching
  70/70 Base C headers and 1037/1037 Base C interfaces with zero Ring 0 changes.
- Added Ring 3 Modern VirtIO 1.0 PCI `virtio-net` driver and TCP/IP/loopback
  network server `netserv` (`/dev/net`) with zero Ring 0 syscall or kernel
  module expansion (`TSK_0041`, `SWR_IPC_0013`, `ADR_0042`, `ICD_IPC_0017`,
  `COMP_IPC_0018`, `VER_IPC_0011`): implemented `libc/include/sys/net.h`,
  `servers/netserv/` (MMIO BARs, 17-page contiguous DMA RX/TX virtqueues, IOAPIC
  IRQ pulses, loopback, Ethernet, ARP, IPv4, IPv6, ICMPv4, ICMPv6, UDP, TCP, and
  poll), QEMU `-netdev user` + `virtio-net-pci` integration, and
  `tests/nettest/test_net.c` verifying hardware ARP/ICMPv4 against SLIRP gateway
  `10.0.2.2` and dual-stack IPv4/IPv6 loopback ICMP/UDP/TCP.
- Added POSIX.1-2024 XCU `grep`, `sed`, `find`, `xargs`, `expr`, and `printf` in
  Ring 3 `bin/utils/` (`TSK_0043`, `SWR_LIBC_0019`, `ADR_0041`,
  `COMP_LIBC_0018`, `VER_LIBC_0019`): implemented BRE/ERE/fixed `grep`, full
  stream-editing `sed` (`-E`, backrefs, hold space, `y///`, branches), `find`
  (`-name`, `-type`, Issue 8 `-print0`, `-exec`), `xargs` (Issue 8 `-0`, `-n`,
  `-I`), `expr` (arithmetic, comparisons, BRE `:`), and `printf` (format reuse,
  `%b`); raised Gate 2 to `68/108` Base utilities (+6, 63.0%); and verified in
  QEMU (`shtest`) with zero Ring 0 changes.
- Completed POSIX.1-2024 XCU shell grammar and standard built-ins in `/boot/sh`
  (`TSK_0042`, `SWR_LIBC_0018`, `ADR_0040`, `COMP_LIBC_0017`, `VER_LIBC_0018`):
  refactored `bin/sh/` into `lexer.c`, `parser.c`, `expand.c`, `exec.c`,
  `builtins.c`, and `main.c`; added compound commands (`if`, `for`, `while`,
  `until`, `case`, subshells, brace groups), shell functions, `$(...)`,
  `$((...))`, parameter expansions, here-documents (`<<`/`<<-`), `<glob.h>`
  globbing, `trap`, and standard built-ins (`.`, `:`, `alias`, `unalias`,
  `command`, `eval`, `exec`, `getopts`, `hash`, `return`, `shift`, `times`,
  `break`, `continue`, `readonly`); raised Gate 2 to `62/108` Base utilities
  (+7, 57.4%); and verified in QEMU (`shtest`) with zero Ring 0 changes.
- Added Ring 3 ELF64 `ET_DYN` dynamic loading and POSIX.1-2024 `<dlfcn.h>`
  (`TSK_0040`, `SWR_LIBC_0017`, `ADR_0039`, `ICD_LIBC_0016`, `COMP_LIBC_0016`,
  `VER_LIBC_0017`): implemented `dlopen`, `dlsym` (`RTLD_DEFAULT`, `RTLD_NEXT`),
  `dlclose`, per-thread `dlerror`, and `dladdr` (`Dl_info_t`) in
  `libc/src/dlfcn.c` using contiguous W^X `mmap`/`mprotect`, `x86-64`
  relocations, SysV/GNU hash symbol tables, and `DT_INIT_ARRAY`/`DT_FINI_ARRAY`
  hooks; added `microkernel_add_user_shared_library` and `libdltest.so` in
  `tests/utilstest/dlplugin.c`; reached 100% of non-network Base headers
  (`65/70`) and `1000/1037` Base interfaces (+5, 96.4%) in Gate 2 with zero Ring
  0 changes.
- Added POSIX.1-2024 `<langinfo.h>`, `<monetary.h>`, `<nl_types.h>`,
  `<iconv.h>`, and `<libintl.h>` plus seven i18n CLI utilities strictly in Ring
  3 (`TSK_0038`, `SWR_LIBC_0016`, `ADR_0038`, `ICD_LIBC_0015`, `COMP_LIBC_0015`,
  `VER_LIBC_0016`): implemented `nl_langinfo*`, `strfmon*`, streaming `iconv`
  (`US-ASCII`/`ISO-8859-1`/`UTF-8`/`UTF-16`/`UTF-32` with `//IGNORE` and
  `//TRANSLIT`), `catopen`/`catgets`/`catclose`, and binary `.mo` `<libintl.h>`
  (`gettext*`/`ngettext*`/`*textdomain*` with `Plural-Forms`); added `locale`,
  `localedef`, `iconv`, `gencat`, `msgfmt`, `gettext`, and `ngettext`; raised
  Gate 2 to `64/70` Base headers (+5), `995/1037` Base interfaces (+25), and
  `55/108` Base utilities (+7); and verified in QEMU (`utilstest`) and CTest
  Gates 1 and 2 with zero Ring 0 changes.
- Added POSIX.1-2024 `<regex.h>`, `<glob.h>`, `<wordexp.h>`, `<tar.h>`, and
  `<cpio.h>` in Ring 3 `libc` (`TSK_0039`, `SWR_LIBC_0015`, `ADR_0037`,
  `ICD_LIBC_0014`, `COMP_LIBC_0014`, `VER_LIBC_0015`): implemented BRE/ERE
  matching (`regcomp`, `regexec`, `regerror`, `regfree`) in `libc/src/regex.c`,
  pathname globbing (`glob`, `globfree`) in `libc/src/glob.c`, shell word
  expansion (`wordexp`, `wordfree`) in `libc/src/wordexp.c`, and archive header
  macros in `<tar.h>` and `<cpio.h>`; raised Gate 2 to `59/70` Base headers (+5)
  and `970/1037` Base interfaces (+8); and verified in QEMU (`utilstest`) and
  CTest Gates 1 and 2 with zero Ring 0 changes.
- Added wide-oriented Ring 3 `FILE` streams, `*wprintf`, `*wscanf`, and
  `open_wmemstream` (`TSK_0045`, `SWR_LIBC_0014`, `ADR_0036`, `ICD_LIBC_0013`,
  `COMP_LIBC_0013`, `VER_LIBC_0014`): extended `libc/src/stdio.c` with `fwide`,
  `open_wmemstream`, `fgetwc`/`fgetws`/`fputwc`/`fputws`/`getwc`/`getwchar`/
  `putwc`/`putwchar`/`ungetwc`, and per-stream `mbstate_t`; implemented
  `fwprintf`/`wprintf`/`swprintf`/`v*wprintf` in `libc/src/wchar_printf.c` and
  `fwscanf`/`wscanf`/`swscanf`/`v*wscanf` in `libc/src/wchar_scanf.c`; completed
  100% of Base `<stdio.h>` and `<wchar.h>` in Gate 2 (`962/1037` Base
  interfaces, +23); and verified in QEMU (`stdiotest`) and CTest Gates 1 and 2
  with zero Ring 0 changes.
- Added POSIX.1-2024 `<locale.h>`, `*_l` locale helpers, `<stdlib.h>` multibyte
  conversions, `<wctype.h>`, and core `<wchar.h>` in Ring 3 `libc` (`TSK_0037`,
  `SWR_LIBC_0013`, `ADR_0035`, `ICD_LIBC_0012`, `COMP_LIBC_0012`,
  `VER_LIBC_0013`): implemented `C` and `C.UTF-8` locales (`setlocale`,
  `localeconv`, `newlocale`, `duplocale`, `freelocale`, `uselocale` over
  `FS.base` TLS, `getlocalename_l`) in `libc/src/locale.c`; added `*_l` helpers
  in `<ctype.h>`, `<strings.h>`, `<string.h>`, and `<time.h>`; added
  `MB_CUR_MAX` and `mblen`/`mbtowc`/`wctomb`/`mbstowcs`/`wcstombs`; added all 36
  `<wctype.h>` functions in `libc/src/wctype.c` and 51 core `<wchar.h>` plus
  `<inttypes.h>` `wcstoimax`/`wcstoumax` functions across
  `libc/src/wchar_{mb,str,conv}.c`; raised Gate 2 to `54/70` Base headers and
  `939/1037` Base interfaces (+123); and verified in QEMU (`libctest`) and CTest
  Gates 1 to 3 with zero Ring 0 changes.
- Added POSIX.1-2024 Base `futimens`/`utimensat`, `lgamma*`, and
  `tzset`/`tzname` in Ring 3 `libc` and `vfsserv` (`TSK_0049`, `SWR_LIBC_0012`,
  `ADR_0034`, `ICD_LIBC_0011`, `COMP_LIBC_0011`, `VER_LIBC_0012`): added
  `UTIME_NOW`, `UTIME_OMIT`, `futimens`, and `utimensat` over `VFS_OP_UTIMENS`
  with distinct `st_atim`/`st_mtim`/`st_ctim` tracking, `ext2_set_times()`
  persistence, and upgraded `bin/utils/touch.c`; added Lanczos `lgamma`,
  `lgammaf`, and `lgammal` in `<math.h>` and `<tgmath.h>`; added `tzset` and
  `tzname[2]` in `<time.h>` without leaking XSI globals; raised Gate 2 to
  `816/1037` Base interfaces; and verified in QEMU (`vfstest`, `mathtest`,
  `timetest`) and CTest Gates 1 to 3 with zero Ring 0 changes.
- Added ISO C17 and POSIX.1-2024 `<complex.h>` and `<tgmath.h>` in Ring 3 `libc`
  (`TSK_0036`, `SWR_LIBC_0011`, `ADR_0033`, `ICD_LIBC_0010`, `COMP_LIBC_0010`,
  `VER_LIBC_0011`): implemented `CMPLX`/`CMPLXF`/`CMPLXL` and all 66 `float`,
  `double`, and `long double` complex functions in `libc/src/complex.c`,
  supplied Annex G `__mul{s,d,x}c3` and `__div{s,d,x}c3` runtime helpers in
  `libc/src/complex_arith.c`, provided pure C17 `_Generic` dispatch in
  `libc/include/tgmath.h`, registered both headers in Gates 1 and 2 (`51/70`
  Base headers, `820/1041` Base interfaces), and verified in QEMU
  `tests/mathtest/test_math.c` with zero Ring 0 changes.
- Completed POSIX.1-2024 Base `<stdio.h>`, `<stdlib.h>`, `<string.h>`, and
  `<time.h>` in Ring 3 `libc` (`TSK_0035`, `SWR_LIBC_0010`, `ADR_0032`,
  `ICD_LIBC_0009`, `COMP_LIBC_0009`, `VER_LIBC_0010`): added `scanf*`
  (`libc/src/stdio_scanf.c`), `getline`/`getdelim`, `dprintf`/`vdprintf`,
  `asprintf`/`vasprintf`, floating-point `printf` formatting, `fmemopen`,
  `open_memstream`, `freopen`, and `fgetpos`/`fsetpos`; added IEEE-754
  `strtod`/`strtof`/`strtold`/`atof` (`libc/src/stdlib_float.c`),
  `aligned_alloc` (`libc/src/malloc.c`), `quick_exit`/`at_quick_exit`,
  `div`/`ldiv`/`lldiv`, `rand`/`srand`, `qsort_r`, and `getsubopt`; added
  `stpcpy`, `stpncpy`, `memmem`, and `strerror_r`; added `time`, `difftime`,
  `clock`, `timespec_get`, `gmtime*`, `localtime*`, `mktime`, `asctime*`,
  `ctime*`, and `strftime` (`libc/src/time_cal.c`) with zero Ring 0 expansion,
  verified in `stdiotest`, `timetest`, and Gates 1 to 3.
- Elevated the minimal Ring 0 microkernel boundary to top-level governance in
  `AGENTS.md`, codified `CNV_0007` and `WDR_0010` (`SWR_DOCS_0006`,
  `VER_DOCS_0007`), and added the automated CTest gate
  `arch.microkernel_boundary` (`tests/arch/test_microkernel_boundary.py` and
  `tests/arch/microkernel_allowlist.json`) enforcing ADR-backed allowlists on
  `kernel/core/*.c` modules, `SYS_*` syscalls, and `struct __k_fd_entry`.
- Documented the headless QEMU test harness architecture, boot modes, and
  failure triage model in `docs/60-verification/qemu-harness.md`, and added the
  `qemu-debug` agent skill (`docs/skills/qemu-debug/SKILL.md`) for monitor
  register inspection, LLVM kernel symbolization, and SMP race isolation.
- Implemented POSIX.1-2024 Base signals, `waitid`, `_Fork`, `popen`/`system`,
  ISO C17 `<threads.h>`, named `MAP_SHARED` semaphores, and XCU utilities
  `sleep`, `time`, `timeout`, `nohup`, and `sh` `wait` (`TSK_0034`,
  `SWR_PROC_0012`, `ADR_0031`, `ICD_LIBC_0008`, `COMP_LIBC_0008`,
  `VER_PROC_0010`): extended `SYS_FUTEX` with physical-frame keying on
  `VMM_SHARED` pages for Ring 3 `sem_open`/`sem_close`/`sem_unlink`, added
  `SYS_SIGTIMEDWAIT` (`64`), `SYS_SIGSUSPEND` (`65`), `SYS_SIGQUEUE` (`66`),
  `THREAD_STOPPED`, and `PROC_WAIT_*` flags, exposed `/bin -> /boot` in
  `vfsserv` and resolved `fexecve` over `VFS_OP_RESOLVE`, added flat
  `siginfo_t`, `<threads.h>` over `<sys/_pthread_types.h>`, LIFO
  `pthread_atfork`, real `PTHREAD_MUTEX_ROBUST` (`EOWNERDEAD` and
  `ENOTRECOVERABLE`), and verified in QEMU and Gates 1 to 3.
- Implemented POSIX.1-2024 Base VFS links, permissions, FIFOs, positional I/O,
  directory enumeration, `<sys/statvfs.h>`, temp-file helpers, and 15 XCU Base
  file utilities (`TSK_0033`, `SWR_IPC_0012`, `ADR_0030`, `ICD_IPC_0016`,
  `COMP_IPC_0017`, `VER_IPC_0010`): extended `vfsserv` (`VFS_OP` `16` to `24`,
  symlink resolution with `ELOOP`, `nlink` reference counting, 4 KiB blocking
  FIFO buffers, `pread`/`pwrite`, `VFS_READDIR_NEXT`, and `statvfs`), added 32
  Base `libc` functions with zero `XSI` leakage, added Ring 3 utilities `chmod`,
  `chown`, `chgrp`, `ln`, `mkfifo`, `readlink`, `realpath`, `touch`, `rmdir`,
  `df`, `du`, `dd`, `basename`, `dirname`, and `pathchk`, and verified in
  `utilstest`, `shtest`, and Gates 1 to 3.
- Implemented POSIX.1-2024 Base process credentials, `<pwd.h>`, `<grp.h>`, and
  `<sys/resource.h>` (`TSK_0032`, `SWR_PROC_0011`, `ADR_0029`, `ICD_PROC_0010`,
  `COMP_PROC_0010`, `VER_PROC_0009`): added `uid`/`euid`/`suid`,
  `gid`/`egid`/`sgid`, supplementary groups, `nice`, and `rlimit` tables on
  `pcb_t` with `SYS_PROC_CRED` (`62`), `SYS_PROC_RLIMIT` (`63`), and kernel
  `RLIMIT_NOFILE` enforcement, added `libc/src/cred.c` (21 Base functions with
  zero `XSI` leakage), added Ring 3 XCU utilities `id`, `logname`, `nice`, and
  `renice` plus `sh` built-ins `umask` and `ulimit`, updated `ps` to resolve
  real UIDs/GIDs and nice values, and verified in `utilstest`, `shtest`, and
  Gates 1 to 3.
- Extended Gate 2 (`posix.symbol_diff`, `tools/posix/audit_lib.py`,
  `tests/posix/test_symbol_diff.py`, `CMakeLists.txt`) to index POSIX.1-2024
  `XCU` Base utilities, verify every binary in `bin/CMakeLists.txt` is packed in
  `boot.tar` and belongs to POSIX.1-2024 Base (`BDR_0004`), and report
  `utilities_base_implemented` plus `## Missing BASE/CX/OB Utilities` in
  `build/posix-symbol-diff.txt`.
- Implemented strict POSIX.1-2024 Base process inspection, configuration, and
  TTY size/name interfaces (`TSK_0031`, `SWR_PROC_0010`, `ADR_0028`,
  `ICD_PROC_0009`, `COMP_PROC_0009`, `VER_PROC_0008`): added `pcb_t.comm`/`args`
  and read-only `FD_OP_PROC_LIST` (`ProcList()` in `<sys/proc.h>`), added
  `sysconf`, `confstr`, `pathconf`, `fpathconf`, `gethostname`, `ctermid`,
  `ttyname`, `ttyname_r`, `tcgetsid`, `tcgetwinsize`, and `tcsetwinsize` with
  `ttyserv` `TTY_OP_{GETSID,GETWINSIZE,SETWINSIZE}` and zero non-POSIX names,
  added Ring 3 XCU utilities `ps`, `getconf`, `tty`, and `stty`, updated Gate 2
  to count standard header macros in `build/posix-symbol-diff.txt`, and verified
  in `utilstest`, `shtest`, and Gates 1 to 3.
- Added business decision `BDR_0004` (`docs/10-product/bdr/BDR_0004.md`) and
  updated `CNV_0004` to keep public user-space headers, `libc`, and utilities
  within POSIX.1-2024 and require explicit `BDR_`/`ADR_` records for any future
  non-POSIX exception, and queued the full dependency-ordered POSIX.1-2024 Base
  backlog (`TSK_0031` to `TSK_0044`) in `docs/records/TASKS.md`.
- Implemented multi-core SMP bring-up and kernel lock hardening (`TSK_0025`,
  `SWR_PROC_0009`, `ADR_0027`, `COMP_PROC_0008`, `VER_PROC_0007`): parsed ACPI
  `MADT` Processor Local APIC entries (`kernel/core/acpi.c`), added portable
  spinlocks (`kernel/include/kernel/spinlock.h`), per-CPU state
  (`kernel/include/kernel/smp.h`, `kernel/core/smp.c`), the `x86-64` 16-bit to
  64-bit `INIT`/`SIPI` AP trampoline (`kernel/arch/x86-64/ap_trampoline.S`),
  per-CPU GDT/TSS/`GS.base`/`swapgs` and `x86-64-v3` `XSAVE`/`XCR0` AP init,
  Reschedule (`0xF0`) and synchronous TLB Shootdown (`0xF1`) IPIs, per-CPU idle
  threads, and spinlock protection across `sched`, `pmm`, `kheap`, `vmm`, `ipc`,
  `proc`, `time`, `signal`, `intr`, `shm`, `entropy`, `cov`, and `printk`,
  exposed `online_cpus`/`cpu_count` in `struct __k_sys_stats`/`boot_info_t`, and
  added Ring 3 `tests/smptest/` plus 4-core CTest `proc.smp_cores`
  (`tests/boot/test_smp_cores.py`).
- Mounted a writable rev-1 `ext2` on-disk filesystem at `/mnt` inside Ring 3
  `vfsserv` over `/dev/blk` (`servers/vfsserv/ext2.{h,c}`,
  `servers/vfsserv/main.c`) supporting automatic format/mount, direct and
  single-indirect blocks, write-through `open`/`read`/`write`/`ftruncate`,
  `mkdir`, `unlink`, `rmdir`, `rename`, `EXDEV`, and unlinked-open inode
  reclamation, with Ring 3 `tests/ext2test/` and CTest `ipc.vfs_persist`
  (`tests/boot/test_ext2_persist.py`) (`SWR_IPC_0011`, `COMP_IPC_0016`,
  `VER_IPC_0009`, `TSK_0028`).
- Added `CAP_SYS_INTR`-gated `SYS_VM_MAP_MMIO`, `SYS_DMA_ALLOC`, and
  `SYS_PCI_CFG` syscalls (`kernel/core/vmm.c`, `kernel/core/intr.c`,
  `kernel/arch/x86-64/arch.c`), Ring 3 wrappers in `libc/include/sys/intr.h`,
  the `/dev/blk` IPC protocol (`libc/include/sys/blk.h`), the Ring 3 Modern
  VirtIO 1.0 PCI block driver (`servers/blkdrv/`), the Ring 3 test
  `tests/blktest/`, and the CTest cross-reboot persistence test
  `ipc.blk_persist` (`tests/boot/test_blk_persist.py`) (`SWR_IPC_0010`,
  `ICD_IPC_0015`, `COMP_IPC_0015`, `VER_IPC_0008`, `TSK_0024`).
- Added POSIX.1-2024 `<fenv.h>` (`libc/arch/x86-64/fenv.c`: `MXCSR` and x87
  rounding modes and exception flags) and `<math.h>`
  (`libc/src/math_{basic,round,exp,trig}.c`: IEEE-754 classification/comparison
  macros and the full Base/CX `float`, `double`, and `long double` math library
  using `x86-64-v3` hardware `FMA`, `vsqrtsd`/`vsqrtss`, and
  `vroundsd`/`vroundss`), registered both headers in Gates 1 and 2, and added
  the Ring 3 QEMU suite `tests/mathtest/` (`SWR_LIBC_0009`, `ICD_LIBC_0007`,
  `COMP_LIBC_0007`, `VER_LIBC_0009`, `TSK_0030`).
- Enforced the `x86-64-v3` minimum CPU baseline at early boot (`CPUID` gate
  halting legacy CPUs cleanly before `x86-64-v3` instructions run), upgraded
  `switch.S` and `sched.c` to dynamically sized 64-byte-aligned `XSAVE`/`XRSTOR`
  context switching with optional `AVX-512` `XCR0` detection, exposed
  `BOOT_CPU_*` flags in `boot_info_t`, compiled Ring 3 with `-march=x86-64-v3`
  and Ring 0 with scalar-only `x86-64-v3` integer instructions (`POPCNT`,
  `TZCNT`/`LZCNT`, `BMI1`/`BMI2`), refactored `pmm.c` (`POPCNT` word reserve and
  contiguous scan), `kernel/lib/string.c` (64-bit scalar word ops), `kheap.c`,
  and `libc/src/string.c` (256-bit `AVX2` `memcpy`/`memmove`/`memset`), and
  added `tests/simdtest/` plus CTest `boot.x86_64_v3` (`SWR_BOOT_0005`,
  `COMP_BOOT_0005`, `VER_BOOT_0004`, `TSK_0029`).

### Changed

- Recorded `ADR_0026` standardizing the minimum `x86-64` CPU baseline on
  `x86-64-v3` (`XSAVE`, `AVX`, `AVX2`, `FMA`, `BMI1`, `BMI2`, with optional
  `AVX-512` detection and `XSAVE`/`XRSTOR` context switching), added draft
  requirements `SWR_BOOT_0005` and `SWR_LIBC_0009`, and queued `TSK_0029` and
  `TSK_0030` (`<math.h>` and `<fenv.h>`) ahead of `TSK_0024`, `TSK_0028`, and
  `TSK_0025` in `docs/records/TASKS.md`.
- Recorded the opt-in automated LLVM code coverage architecture (`ADR_0025`,
  draft `SWR_DOCS_0005`) and queued `TSK_0027` as the next task in
  `docs/records/TASKS.md` (using freestanding Raw Profile v8 over portable
  `arch_console_write()` base64 framing and `./scripts/env.sh coverage` in
  isolated `build-cov/`).
- Reordered `docs/records/TASKS.md` by dependency (`TSK_0026` -> `TSK_0023` ->
  `TSK_0022` -> `TSK_0024` -> `TSK_0025`) and refined `SWR_PROC_0008`
  (`waitpid(0)`/`-pgid`, `kill(-pgid)`) and `SWR_IPC_0010` (`CAP_SYS_INTR`
  MMIO/DMA syscalls).
- Defined the platform abstraction architecture (`ADR_0024`), added `RDM_0008`
  in `docs/records/ROADMAP.md` for future 64-bit ARM (`arm64`) support (no
  legacy 32-bit `x86`), queued `TSK_0026` (`SWR_BOOT_0004`) to isolate `x86-64`
  platform code behind `x86-64` HAL folders, and updated `README.md` and
  `docs/10-product/vision.md` to state `x86-64` as the current active platform.
- Added a Mermaid documentation guideline to `AGENTS.md` instructing agents to
  include purposeful, vertical (`flowchart TD` / `direction TB`), timeless
  Mermaid diagrams where helpful to illustrate documented systems and concepts.
- Refined `TSK_0020` (`SWR_DOCS_0004`) in `docs/records/TASKS.md` with Gate 1–3
  `<sys/select.h>` coverage and quantitative PMM/server-handle leak checks, and
  queued the next development phase (`TSK_0022` TTY job control/`ISIG`/core
  utilities, `TSK_0023` high-leverage `libc` Base headers, `TSK_0024` Ring 3
  `virtio-blk` persistent storage, and `TSK_0025` ACPI `MADT` SMP bring-up) with
  matching draft requirements (`SWR_PROC_0008`–`0009`, `SWR_LIBC_0008`,
  `SWR_IPC_0010`).
- Audited and updated all `POSIX` references across documentation, headers, code
  comments, and audit scripts to use work-in-progress, goal-oriented wording
  targeting `POSIX.1-2024` (`IEEE Std 1003.1-2024` / `ISO/IEC/IEEE 9945:2026`),
  remove compliance or certification phrasing, keep `POSIX` out of titles and
  component names, add IEEE trademark attribution to public-facing documentation
  (`README.md`, `docs/00-overview/glossary.md`,
  `docs/60-verification/traceability.md`, `docs/THIRD-PARTY-NOTICES.md`), and
  codify these rules in `AGENTS.md`.
- Added Mermaid architecture, sequence, and state diagrams across `README.md`,
  `docs/00-overview/context.md`, and all domain architecture pages
  (`docs/30-architecture/{boot,mem,proc,ipc,sig,libc,docs}.md`) illustrating
  Ring 0/Ring 3 topology, boot bring-up, x86-64 memory map, COW faults, thread
  states, synchronous channel IPC, IRQ pulses, multi-server `ppoll()`, signal
  frames, three-layer POSIX `libc`, and bidirectional traceability.
- Added follow-up task `TSK_0021` (`SWR_BOOT_0003`) to `docs/records/TASKS.md`
  for ACPI `MADT`/`SPCR` hardware discovery, IOAPIC ISO routing, parameterized
  Ring 3 `ttyserv`/`rtcdrv`, and user `OPOST` TX ownership before final
  hardening (`TSK_0020`).
- Refined and dependency-ordered the remaining backlog in
  `docs/records/TASKS.md` (`TSK_0012` -> `TSK_0016` -> `TSK_0019` -> `TSK_0018`
  -> `TSK_0017` -> `TSK_0020`), adding draft requirements `SWR_LIBC_0007`
  (buffered `<stdio.h>`, `setenv`/`unsetenv`, `execvp`, core headers) and
  `SWR_DOCS_0004` (`<sys/select.h>` and cross-subsystem stress hardening).
- Removed volatile test and assertion counts from the `README.md` Mermaid
  architecture diagram and workflow overview so the diagram stays focused on
  tools and technologies.
- Included the root `README.md` dynamically as the top section of the generated
  Sphinx-Needs HTML home page (`docs/index.md` via
  `config/sphinx/readme_include.py`) with automatic Sphinx link rewriting and
  Mermaid diagram rendering.
- Updated `README.md` to document the closed-loop AI agent development cycle
  (Blueprint & bidirectional Sphinx-Needs traceability, Podman/LLVM Fabricator,
  headless QEMU Testbed, and automated Quality Gates) with a Mermaid flowchart.
- Extended and dependency-ordered the task backlog in `docs/records/TASKS.md`
  (`TSK_0013`, `TSK_0014`, `TSK_0015`, `TSK_0012`, `TSK_0016`, `TSK_0017`,
  `TSK_0018`) with matching draft requirements (`SWR_MEM_0006`, `SWR_IPC_0007`–
  `SWR_IPC_0009`, `SWR_LIBC_0006`).
- Consolidated all legacy `agent-scaffolding` documents (`docs/TASKS.md`,
  `docs/DECISIONS.md`, `docs/CONVENTIONS.md`, `docs/VISION.md`, and
  `docs/ARCHITECTURE.md`) into the Sphinx-Needs traceability model (`WDR_0003`–
  `WDR_0009`, `ADR_0004`–`ADR_0010`, `CNV_0005`–`CNV_0006`, `RDM_0004`–
  `RDM_0007`, `docs/00-overview/context.md`, and `docs/10-product/vision.md`),
  redirected `AGENTS.md`, `README.md`, and `docs/skills/` to the traceability
  records, retained `docs/THIRD-PARTY-NOTICES.md`, and removed the five legacy
  files (`TSK_0008`, `WDR_0009`).

### Fixed

- Fixed a multi-core futex self-deadlock in `libc/src/malloc.c` (`heap_lock()`)
  where `__atomic_exchange_n(&g_heap_lock, 2u)` could acquire an unlocked mutex
  (`0 -> 2`) and fall through to `FUTEX_WAIT(2)` on its own lock, and reserved
  well-known `chanid 1` for PID 1 (`sysproc`) in `kernel/core/channel.c` to
  prevent SMP boot-time channel ID races.
- Fixed process current working directory (`cwd`) and file creation mask
  (`umask`) persistence across `execve()` and `fork()` by storing `cwd` and
  `umask` in the kernel `pcb_t` (`kernel/include/kernel/sched.h`,
  `kernel/core/sched.c`, `kernel/core/proc.c`) and exposing `FD_OP_GETCWD`,
  `FD_OP_SETCWD`, and `FD_OP_UMASK` through `SYS_FD_OP`
  (`libc/include/sys/fd.h`, `libc/src/vfs.c`), so utilities such as `ls` operate
  on the shell working directory after `cd` instead of resetting to `/`, with
  regression tests in `tests/shtest/test_sh.c`.
- Fixed strict priority enforcement in `kernel/core/sched.c` (`schedule()`) so a
  running higher-priority thread (such as `sysproc` at priority 110 during boot)
  is never preempted by a lower-priority ready thread when its round-robin time
  slice expires.

### Added

- TTY job control, `ISIG` (`Ctrl-C`/`Ctrl-\`), process groups, sessions, and
  Ring 3 core utilities (`TSK_0022`, `SWR_PROC_0008`, `ICD_PROC_0008`,
  `COMP_PROC_0007`, `VER_PROC_0006`): added `SYS_PROC_PGID` (`setpgid`,
  `getpgid`, `getpgrp`, `setsid`, `getsid`), `SYS_KILL` (`kill(-pgid)` /
  `kill(0)` / `kill(-1)`), `waitpid(0)`/`waitpid(-pgid)`, `ttyserv`
  `TTY_OP_GETPGRP`/`TTY_OP_SETPGRP` (`tcgetpgrp`/`tcsetpgrp`) and `ISIG`
  `VINTR`/`VQUIT` delivery, interactive `/boot/sh` foreground job groups, 8 core
  utilities (`mkdir`, `rm`, `cp`, `mv`, `uname`, `env`, `wc`, `head`), and QEMU
  test `tests/jobtest/test_job.c`. Also split persistent storage backlog into
  `TSK_0024` (`SWR_IPC_0010`, MMIO/DMA + `virtio-blk`) and `TSK_0028`
  (`SWR_IPC_0011`, writable on-disk filesystem mount in `vfsserv`).
- Additional POSIX.1-2024 Base headers and standard C utilities (`TSK_0023`,
  `SWR_LIBC_0008`, `ICD_LIBC_0006`, `COMP_LIBC_0006`, `VER_LIBC_0008`):
  registered 7 compiler freestanding headers (`<float.h>`, `<iso646.h>`,
  `<stdarg.h>`, `<stdbool.h>`, `<stddef.h>`, `<stdint.h>`, `<stdnoreturn.h>`)
  and added `<sys/utsname.h>` (`uname`), `<endian.h>` (`htobe*`/`be*toh`/
  `htole*`/`le*toh`), `<sched.h>` (`sched_yield` via `SYS_SCHED_YIELD`),
  `<setjmp.h>` (`setjmp`/`longjmp` in `libc/arch/x86-64/setjmp.S`,
  `sigsetjmp`/`siglongjmp`), `<sys/times.h>` (`times` via `SYS_PROC_TIMES`),
  `<fnmatch.h>` (`fnmatch`), `atexit`, `qsort`, `bsearch`, `getopt`, `alarm`
  (via `SYS_ALARM`), `rename`/`renameat` (via `VFS_OP_RENAME`), `realpath`, and
  QEMU test `tests/utilstest/test_utils.c` (bringing Gate 1–2 Base headers from
  29/70 to 42/70).
- Opt-in automated LLVM source-based code coverage pipeline (`TSK_0027`,
  `SWR_DOCS_0005`, `ADR_0025`, `ICD_DOCS_0002`, `COMP_DOCS_0004`,
  `VER_DOCS_0006`): added `-DMICROKERNEL_COVERAGE=ON` instrumentation for Ring 0
  and Ring 3 (`kernel/`, `libc/`, `servers/`, `bin/`, `tests/`), linker
  `__llvm_prf_*` section anchors, freestanding Raw Profile v8 streaming over
  `arch_console_write()` (`SYS_COV_WRITE`), `tests/test_runner.py --cov-out
  --wait-cov`, `tools/coverage/generate_report.py`, CTest `coverage.unit`,
  `./scripts/env.sh coverage` (generating `build-cov/coverage/html/index.html`,
  `lcov.info`, and `summary.txt`), and `build-cov/` in `.gitignore`.
- `x86-64` Hardware Abstraction Layer (HAL) isolation and parameterized platform
  build (`TSK_0026`, `SWR_BOOT_0004`, `ADR_0024`, `ICD_BOOT_0002`,
  `COMP_BOOT_0004`, `VER_BOOT_0003`): moved `x86-64` kernel, linker, `libc`
  CRT/syscall/TLS/port-I/O, and Ring 3 UART/RTC hardware backends into `x86-64`
  folders (`kernel/arch/x86-64/`, `libc/arch/x86-64/`,
  `servers/ttyserv/arch/x86-64/`, `servers/rtcdrv/arch/x86-64/`,
  `cmake/arch/x86-64.cmake`), added portable `kernel/include/kernel/arch.h` and
  trapframe accessors, and parameterized CMake (`-DMICROKERNEL_ARCH=x86-64`),
  `scripts/env.sh`, `tests/test_runner.py`, and POSIX audit gates (`--target`
  and `AArch64`-ready relocation/branch regexes).
- `<sys/select.h>` (`select`, `pselect`, `fd_set`, `FD_*`), kernel zombie
  reclamation, and cross-subsystem zero-leak stress suite (`TSK_0020`,
  `SWR_DOCS_0004`, `ADR_0023`, `ICD_IPC_0014`, `COMP_IPC_0014`,
  `VER_DOCS_0005`): added standalone `libc/include/sys/select.h` (`suseconds_t`,
  `struct timeval`, `FD_SETSIZE`), `libc/src/select.c` over multi-server
  `ppoll()` with cancellation points, Gate 1–3 checks in `posix.headers`,
  `posix.symbol_diff`, and `posix.cancellation`, kernel zombie process and
  thread stack/TCB/FPU reclamation (`proc_reap_zombie()`,
  `proc_reap_zombie_threads()`, slot-reusing `proc_user_stack_alloc()`),
  `SYS_SYS_STATS` (`52`) and `PIPE_OP_STATS`/`VFS_OP_STATS`/`SOCK_OP_STATS`
  RPCs, and QEMU `tests/stresstest/test_stress.c`.
- ACPI `MADT`/`SPCR` hardware discovery, parameterized Ring 3 drivers, Ring 3
  `OPOST`/`ECHO` TX, and interactive QEMU shell boot (`SWR_BOOT_0003`,
  `ADR_0022`, `COMP_BOOT_0003`, `VER_BOOT_0002`): added `kernel/core/acpi.c`
  parsing RSDP/RSDT/XSDT, `MADT` IOAPIC base and ISOs, and optional `SPCR`,
  ISO-aware IOAPIC setup in `kernel/core/intr.c`, `spawn_image_argv()` and
  `init=sh` interactive boot mode in `kernel/core/proc.c`, `-p`/`-i` driver
  arguments in `servers/ttyserv/main.c` and `servers/rtcdrv/main.c`, Ring 3
  16550A `OPOST`/`ONLCR`/`ECHO` TX routing via `/dev/tty` in `libc/src/tty.c`
  and `libc/src/fd.c`, `./scripts/env.sh run` interactive launcher, and user
  instructions in `README.md`.
- Ring 3 command shell (`sh`) and core utilities targeting `POSIX.1-2024` XCU
  (`SWR_LIBC_0006`, `ADR_0021`, `COMP_LIBC_0005`, `VER_LIBC_0007`): added
  `bin/sh/main.c` (`-c`, script files, interactive `isatty()` prompt, pipelines
  over `pipe2`/`fork`/`dup2`/`execvp`, redirections `<`/`>`/`>>`/`2>`/`2>&1`,
  `&&`/`||`/`;`, `$VAR`/`$?`, Issue 8 `$'...'` ANSI-C quoting, `set -o
  pipefail`, `read -d` custom and NUL delimiters, built-ins `cd`/`pwd`/`export`/
  `unset`/`exit`), standalone `bin/utils/{echo,cat,ls,true,false,test}.c`, `bin`
  scanning in formatter and lint scripts, and QEMU `tests/shtest/test_sh.c`.
- Ring 3 `AF_UNIX` local sockets server and POSIX.1-2024 `accept4`
  (`SWR_IPC_0009`, `ADR_0020`, `ICD_IPC_0012`–`0013`, `COMP_IPC_0012`–`0013`,
  `VER_IPC_0007`): added `servers/sockserv/main.c` (`/dev/sock`) serving
  `AF_UNIX` `SOCK_STREAM` and `SOCK_DGRAM` sockets with deferred blocking
  `accept`/`send`/`recv`/`ppoll` queues, `FD_KIND_SOCK` close-pulse support in
  `kernel/core/proc.c`, standalone `<sys/socket.h>` and `<sys/un.h>`,
  async-signal-safe `libc/src/socket.c` (`socket`, `socketpair`, `bind`,
  `listen`, `accept`, `accept4` with atomic `SOCK_CLOEXEC`/`SOCK_NONBLOCK`,
  `connect`, `send`, `recv`, `sendto`, `recvfrom`, `shutdown`, `getsockname`,
  `getpeername`, `getsockopt`, `setsockopt`), multi-server `ppoll()` dispatch in
  `libc/src/fd.c`, and QEMU `tests/socktest/test_sock.c`.
- Buffered POSIX.1-2024 `<stdio.h>`, environment mutation, `execvp`, and core
  C17/POSIX headers (`SWR_LIBC_0007`, `ADR_0019`, `ICD_LIBC_0005`,
  `COMP_LIBC_0004`, `VER_LIBC_0006`): added standalone `<ctype.h>`,
  `<assert.h>`, `<inttypes.h>`, `<strings.h>`, and `<limits.h>` (with C17 and
  POSIX runtime limits), buffered `FILE` streams (`fopen`, `fdopen`, `fclose`,
  `fread`, `fwrite`, `fgets`, `fgetc`, `ungetc`, `fseeko`, `ftello`, `fileno`,
  `setvbuf`, `vsnprintf`, `vfprintf`, `perror`), `setenv`/`unsetenv`,
  `execv`/`execvp` with `/boot/` `bootfs` lookup, `strdup`/`strndup`/`strstr`,
  `strrchr`/`memchr`/`strtok_r`/`strerror`, and QEMU
  `tests/stdiotest/test_stdio.c`.
- Ring 3 serial TTY server and multi-server `ppoll()` (`SWR_IPC_0008`,
  `ADR_0018`, `ICD_IPC_0010`–`0011`, `COMP_IPC_0010`–`0011`, `VER_IPC_0006`):
  added `servers/ttyserv/main.c` (`/dev/tty`) driven by COM1 IRQ 4 IOAPIC pulses
  with canonical (`ICANON`, `VERASE`, `VKILL`, `ICRNL`, `VEOF`) and raw (`VMIN`)
  line discipline, standalone `<termios.h>`, `isatty`, async-signal-safe
  `libc/src/tty.c` (`tcgetattr`, `tcsetattr`, `tcflush`, `tcdrain`, `tcflow`,
  `tcsendbreak`, `cf*speed`), multi-server `ppoll()` routing in `libc/src/fd.c`,
  and QEMU `tests/ttytest/test_tty.c`.
- Option-gated POSIX.1-2024 memory extensions (`SWR_MEM_0005`, `ADR_0017`,
  `COMP_MEM_0005`, `VER_MEM_0004`): added `_POSIX_SHARED_MEMORY_OBJECTS`,
  `_POSIX_MEMLOCK`, and `_POSIX_MEMLOCK_RANGE` (`202405L`) in
  `libc/include/sys/_posix_opts.h`, kernel shared-frame `MAP_SHARED` and
  `SYS_SHM_OP`/`SYS_SHM_MAP`/`FD_KIND_SHM` in `kernel/core/shm.c`, `VMM_SHARED`
  and `VMM_LOCKED`/`SYS_VM_MLOCK` in `kernel/core/vmm.c`, `shm_open`,
  `shm_unlink`, `mlock`, `munlock`, `mlockall`, and `munlockall` in
  `libc/src/mman.c`, `ftruncate`/`fstat` SHM support in `libc/src/vfs.c`, and
  QEMU `tests/shmtest/test_shm.c`.
- Ring 3 VFS server (`SWR_IPC_0007`, `ADR_0016`, `ICD_IPC_0007`–`0009`,
  `COMP_IPC_0008`–`0009`, `VER_IPC_0005`): added `servers/vfsserv/main.c`
  (`/dev/vfs`) serving read-only `boot.tar` at `/boot` and writable dynamic
  `tmpfs` at `/` and `/tmp`, read-only `BOOTFS_USER_VA` mapping and
  `FD_KIND_VFS` close-pulse support in `kernel/core/proc.c`, `<sys/stat.h>`,
  `<dirent.h>`, `libc/src/vfs.c` (`open`, `openat`, `creat`, `lseek`,
  `ftruncate`, `stat`, `fstat`, `lstat`, `fstatat`, `mkdir`, `mkdirat`,
  `unlink`, `unlinkat`, `rmdir`, `chdir`, `fchdir`, `getcwd`, `umask`, `access`,
  `opendir`, `fdopendir`, `readdir`, `rewinddir`, `closedir`, `dirfd`), and QEMU
  `tests/vfstest/test_vfs.c`.
- Thread-safe `mmap`/`munmap`-backed heap allocator (`SWR_MEM_0006`, `ADR_0015`,
  `COMP_MEM_0004`, `VER_MEM_0003`): replaced the static 1 MiB bump arena in
  `libc/src/stdlib.c` with `libc/src/malloc.c` featuring a `SYS_FUTEX` lock, 64
  KiB coalescing slabs with whole-slab `munmap` reclamation, direct
  `mmap`/`munmap` for allocations >= 32 KiB, in-place `realloc` growth and
  splitting, POSIX.1-2024 `errno`-preserving `free`, and QEMU
  `tests/heaptest/test_heap.c`.
- Complete 4-gate POSIX.1-2024 compatibility harness (`SWR_DOCS_0003`,
  `COMP_DOCS_0003`, `VER_DOCS_0004`): added CTest Gate 2 `posix.symbol_diff`
  (`tests/posix/test_symbol_diff.py` comparing Clang JSON AST declarations and
  `llvm-nm` `libc.a` exports against `tests/posix/issue8-manifest.tsv` with diff
  report generation) and Gate 3b `posix.cancellation`
  (`tests/posix/test_cancellation.py` auditing XSH 2.9.5 cancellation call-graph
  reachability), expanded `posix.async_signal_safe` forbidden callees to include
  `realloc*`, `<stdio.h>` stream locks, and mutex locks, and centralized
  call-graph and manifest helpers in `tools/posix/audit_lib.py`.
- POSIX.1-2024 Base library functions and C17 headers (`SWR_LIBC_0005`,
  `ADR_0014`): kernel `SYS_GETENTROPY` (`48`) mixer (`kernel/core/entropy.c`),
  `getentropy` (`GETENTROPY_MAX = 256`), `strlcpy`, `strlcat`, `realloc`,
  `reallocarray`, `_Exit`, `getenv`, `secure_getenv`, `<uchar.h>` UTF-16/UTF-32
  conversions (`libc/src/uchar.c`), standalone `<stdatomic.h>`/`<stdalign.h>`
  verification, and QEMU `tests/libctest/test_libc_issue8.c`.
- POSIX.1-2024 Base thread cancellation points and recursive `<stdio.h>` stream
  locking (`SWR_LIBC_0004`, `ADR_0013`): `pthread_cancel`,
  `pthread_setcancelstate`, `pthread_setcanceltype`, `pthread_testcancel`,
  `pthread_cleanup_push`/`pop`, internal `LIBC_SIGCANCEL` wakeup across
  `pthread_cond_wait`, `read`, `write`, `close`, `ppoll`, `sem_wait`,
  `clock_nanosleep`, `waitpid`, `aio_suspend`, and `pthread_join`, plus `FILE`
  (`stdin`/`stdout`/`stderr`), `flockfile`, `ftrylockfile`, `funlockfile`,
  `fprintf`, `fputs`, `fputc`, `putchar_unlocked`, `fflush`, and QEMU
  `tests/canceltest`.
- POSIX.1-2024 Base asynchronous I/O (`SWR_IPC_0005`, `ADR_0012`): `<aio.h>`,
  `O_SYNC`/`O_DSYNC` in `<fcntl.h>`, Ring 3 worker-thread engine
  (`libc/src/aio.c`) with `aio_read`, `aio_write`, `aio_fsync`, `aio_cancel`,
  `lio_listio`, `SIGEV_SIGNAL`, async-signal-safe `aio_error`/`aio_return`/
  `aio_suspend`, QEMU `tests/aiotest`, and CTest header/safety coverage.
- POSIX.1-2024 Base `fork`, `execve`, `pipe2`, `dup3`, `fcntl`, `poll`, `ppoll`,
  and `sigpending` (`TSK_0007`, `SWR_PROC_0004`, `SWR_IPC_0004`): syscalls
  `SYS_PROC_FORK` (`42`), `SYS_PROC_EXECVE` (`43`), `SYS_PROC_GETPID` (`44`),
  `SYS_PROC_GETPPID` (`45`), `SYS_FD_OP` (`46`), and `SYS_SIGPENDING` (`47`),
  kernel per-process FD metadata with `PULSE_CODE_FD_CLOSE` cleanup, Ring 3
  `servers/pipeserv` (`/dev/pipe`), `<fcntl.h>`, `<poll.h>`, async-signal-safe
  `libc/src/fd.c`, QEMU `tests/forkpipetest`/`exechelper`, and CTest coverage
  (`ADR_0011`).
- POSIX.1-2024 Base `<pthread.h>` and `<semaphore.h>` plus hardware x86-64
  `FS.base` TLS (`TSK_0006`, `SWR_PROC_0005`): syscalls `SYS_THREAD_SET_FSBASE`
  (`39`), `SYS_THREAD_EXIT` (`40`), and `SYS_FUTEX` (`41`), O(1) TLS `errno`,
  mutexes, `pthread_cond_clockwait`, barriers, spinlocks, rwlocks, TSD keys,
  async-signal-safe `sem_post`, QEMU `tests/pthreadtest`, and CTest coverage
  (ADR-19).
- POSIX.1-2024 Base monotonic/realtime clocks and interval timers (`TSK_0005`,
  `SWR_PROC_0006`): syscalls `30`–`38`, `kernel/core/time.c`, `<time.h>`,
  async-signal-safe `libc/src/time.c`, `sleep()` backed by `clock_nanosleep`,
  QEMU `tests/timetest`, and `posix.headers`/`posix.async_signal_safe` coverage
  (ADR-18).
- Unified pre-commit quality gate (`tools/check_ste.py`, updated
  `scripts/build-agent.sh`, and `scripts/install-hooks.sh`) enforcing full-repo
  `clang-format`, `docs-traceability-setup` `check_text.py --root .`, optional
  `ste-writing-skill`, build, QEMU tests, `clang-tidy`, and CTest before every
  commit (ADR-17).
- Milestone 6 (`TSK_0004`) hardware interrupt, port I/O, and scatter-gather IPC
  syscalls: `SYS_INTR_ATTACH_EVENT` (`11`), `SYS_INTR_UNMASK` (`12`),
  `SYS_IOPL_SET` (`25`, `CAP_SYS_INTR`-guarded), `SYS_MSG_READV` (`8`), and
  `SYS_MSG_WRITEV` (`9`), backed by IOAPIC routing (`kernel/core/intr.c`) and
  pre-allocated IRQ IPC pulses.
- Public `<sys/intr.h>` header (`AttachIntrEvent`, `IntrUnmask`, `IOPLSet`,
  `port_in*`/`port_out*`), `ReadMsgV`/`WriteMsgV`/`ipc_iov_t` in `<sys/ipc.h>`,
  Ring 3 MC146818 CMOS RTC driver (`servers/rtcdrv`, `/dev/rtc`), and
  unprivileged client test (`tests/drvtest`).
- Milestone 5 (`TSK_0002`) process and memory syscalls: `SYS_PROC_CREATE`,
  `SYS_PROC_WAIT` (`28`), `SYS_VM_MAP`, `SYS_VM_UNMAP`, `SYS_VM_CLONE_COW`, and
  `SYS_VM_PROTECT` (`29`), backed by per-frame PMM reference counts, Ring 3
  `#PF` COW fault handling, and user PML4 reclamation on child reap.
- Public POSIX.1-2024 `<sys/mman.h>` (`mmap`, `munmap`, `mprotect`, `msync`) and
  `<sys/wait.h>` (async-signal-safe `wait`, `waitpid`, `W*` macros) plus native
  `<sys/proc.h>` and `<sys/vm.h>` headers and `libc` wrappers.
- Automated Milestone 5 verification (`tests/proctest/main.c` and `child.c`)
  running concurrently with Milestones 1–4 in QEMU, plus `<sys/mman.h>` /
  `<sys/wait.h>` coverage in `posix.headers` and `wait`/`waitpid` in
  `posix.async_signal_safe`.
- Standard POSIX.1-2024 `<signal.h>` (`libc/include/signal.h`) and
  async-signal-safe `libc/src/signal.c` implementing `sigaction`, `sigprocmask`,
  `pthread_sigmask`, `kill`, `pthread_kill`, `raise`, `signal`, and `sigset_t`
  helpers, plus kernel `SYS_SIGPROCMASK` (`27`) and `sig_frame_t.saved_mask`
  save/restore across `SYS_SIGRETURN`.
- Gate 3 CTest `posix.async_signal_safe`
  (`tests/posix/test_async_signal_safe.py`) auditing `sysroot/usr/lib/libc.a`
  with `llvm-objdump -dr`, plus `<signal.h>` standalone coverage in
  `posix.headers` and QEMU runtime checks in `tests/sigtest/main.c`.
- Two-tier bidirectional code and POSIX.1-2024 traceability: `:posix:` field and
  calculated progress matrix in `docs/60-verification/traceability.md`,
  bidirectional `ICD_`/`COMP_`/`VER_` coverage check in
  `config/sphinx/need_tag_check.py`, `WDR_0002`, `ICD_`/`COMP_` code tags across
  all active `kernel/`, `libc/`, `servers/`, and traceability tooling files, and
  governance rules in `AGENTS.md`.
- `<errno.h>` and lock-free TID-indexed thread-local `errno`
  (`libc/src/errno.c`) with the target POSIX.1-2024 `E*` constant set in
  `libc/include/sys/errno.h`.
- Standalone POSIX.1-2024 header compilation and namespace hygiene CTest gate
  (`posix.headers` via `tests/posix/test_headers.py`) and multithreaded `errno`
  verification in `tests/sigtest/main.c`.
- POSIX.1-2024 (Issue 8) Base and option-group software requirements across
  `LIBC`, `SIG`, `MEM`, `PROC`, `IPC`, and `DOCS` with canonical `9799919799`
  specification URLs, plus `ADR_0003`, `CNV_0004`, and `TSK_0003`.
- Real traceability domains (`BOOT`, `MEM`, `PROC`, `IPC`, `SIG`, `LIBC`, and
  `DOCS`) across all Sphinx-Needs layers, plus `BDR_0002`, `BDR_0003`
  (POSIX.1-2024 Base compatibility target), `ADR_0002`, and `microkernel` tasks
  and roadmap records.
- The docs-as-code traceability model with the docs-traceability-setup agent
  skill 0.1.0. The configuration is in `.docs-traceability-setup.yml`.
- The Sphinx documentation in `docs/` with MyST-Parser and Sphinx-Needs, built
  by CMake with warnings as errors and checked by CTest (unit tests, HTML build,
  needs export, model audit).
- The traceability example items for the `POWER` domain, the decision records,
  and the project records in `docs/records/`.
- The EditorConfig file `.editorconfig` with the file header notice lines, and
  the Python dependency manifest `requirements.txt`.

### Changed

- Added `${CMAKE_SOURCE_DIR}/kernel` to `kernel.elf` include directories in
  `CMakeLists.txt` and replaced all parent-relative `../arch/x86_64/` includes
  with `<arch/x86_64/...>` across the kernel.
- Updated `scripts/install-hooks.sh` so `.git/hooks/pre-commit` also runs full
  CTest (`traceability.unit`, `docs.html`, `docs.needs`, `docs.audit`,
  `posix.headers`, `posix.async_signal_safe`) with fresh `build/docs/doctrees`.
- Expanded `docs/00-overview/glossary.md` to explain Ring 0, Ring 3, syscalls,
  PCB/TCB, IPC, PMM/VMM, PML4, SMEP/SMAP, COW, bootfs, sysroot, and
  async-signal-safety.
- Updated `docs/TASKS.md` and `docs/records/TASKS.md` with near-term open tasks
  (`TSK_0004`–`TSK_0007`) covering Milestone 6 and the next POSIX.1-2024 Base
  building blocks, and archived completed `RDM_0001` into
  `docs/records/HISTORY.md`.
- Updated `README.md` with a user-space functionality support table, current
  application use-cases, and an overview of the bidirectional Sphinx-Needs
  traceability setup, and added a rule in `AGENTS.md` requiring agents to keep
  the `README.md` support breakdown up to date.
- Updated `README.md` to highlight the POSIX.1-2024 (Issue 8) Base compatibility
  target, CTest commands, and links to `BDR_0003`, `ADR_0003`, and the
  traceability matrix.
- Aligned `.clang-format` (`ColumnLimit: 80`) with `.editorconfig`, added
  `.editorconfig` exemptions for verbatim `LICENSE.md` and immutable
  `docs/skills/*/references/*`, upgraded all existing C/H, CMake, shell, and
  Python files to Doxygen headers, and wrapped pre-existing docs so full-repo
  `check_text.py --root .` passes with zero findings.
- Mandated `.editorconfig` and `docs-traceability-setup` `check_text.py` checks
  (line width, greedy prose packing, and Doxygen file headers) in `AGENTS.md`,
  `WDR_0002`, and `CNV_0002`, and wrapped `AGENTS.md` to 80 columns.
- Switched `authoring.modal_verb` in `.docs-traceability-setup.yml` from `shall`
  to `must`, revised all requirements in `docs/20-system/` using the
  `ste-writing-skill` skill, expanded `technical_terms.txt` and
  `allowed_phrases.txt`, and documented technical writing rules in `AGENTS.md`,
  `WDR_0002`, and `CNV_0002`.
- Retargeted in-code `VER_` tags in `tests/test_runner.py`,
  `tools/traceability/check_needs_audit.py`, and `tests/traceability/` from
  `VER_POWER_*` to the real `BOOT`, `MEM`, `PROC`, `IPC`, `SIG`, `LIBC`, and
  `DOCS` verification items.
- The root `CMakeLists.txt` builds the documentation with
  `add_subdirectory(docs)` and enables CTest.
- The build container adds the `python3-venv` and `python3-pip` packages for the
  documentation venv.
- The headless QEMU test harness `tests/test_runner.py` carries the
  `VER_POWER_0005` traceability tag.

### Removed

- The `POWER` placeholder domain documents (`docs/*/power.md`) from the
  traceability model.

### Fixed

- Fixed `schedule()` idle loop in `kernel/core/sched.c` to enable interrupts
  (`sti; hlt; cli`), re-poll `pick_next()`, pop woken self-threads properly, and
  avoid recursive `sched_tick()` preemption when all threads are blocked.
- Reserved Multiboot2 boot modules (`bootfs`) in `pmm_init()` so dynamic frame
  allocations never overwrite the boot archive needed by `ProcCreate`.
- Fixed `ISR_STUB` in `kernel/arch/x86_64/isr.S` to pop all 16 bytes (`vec` and
  `err`) before `iretq` on error-code exceptions such as recoverable `#PF` COW
  page faults.
- Aligned secondary Ring 3 thread entry stacks in `sys_thread_reset` to satisfy
  the System V AMD64 `RSP % 16 == 8` C function entry invariant, preventing
  `#GP` faults on SSE `movaps` instructions in spawned worker threads.

### Added

- Scaffolding with the agent-scaffolding agent skill 0.2.0. The configuration is
  in `.agent-scaffolding.yml`.
- The MIT License in `LICENSE.md`, with copyright headers and SPDX identifiers
  in the source files.
- Governance and state documents: `AGENTS.md`, `docs/VISION.md`,
  `docs/DECISIONS.md`, `docs/CONVENTIONS.md`, `docs/THIRD-PARTY-NOTICES.md`, and
  this changelog.
- Agent skills in `docs/skills/`, with the Conventional Commits, SemVer, and
  Agent Skills specifications as references.
- A containerized development environment wrapper `scripts/enter-dev.sh`, and
  build and formatter agent scripts with their agent definitions in
  `docs/agents/`.
- Clang tool configurations `.clang-format` and `.clang-tidy`, verified in the
  build container.

### Changed

- The task plan moved from `TASKS.md` at the repository root to `docs/TASKS.md`.
- The test harness and the test applications moved to `tests/`:
  `tests/test_runner.py`, `tests/ipccli/`, and `tests/sigtest/`.
