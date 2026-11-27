## Purpose

This file records the important changes of `microkernel`. The format obeys [Keep
a Changelog](https://keepachangelog.com).

## Structure

- The `## [Unreleased]` section is at the top. Releases follow that section in
  newest-first sequence (git commit sequence).
- Each section uses level 3 category titles: `### Added`, `### Changed`, `###
  Deprecated`, `### Removed`, `### Fixed`, and `### Security`. Put a title into
  this file only when the section has content for that title.

## Format Rules

- Use `## [x.y.z] - YYYY-MM-DD` for each release title.
- Put the entries in each category in newest-first sequence (git commit
  sequence).
- Write each entry as one or more full sentences with a period at the end.

## [Unreleased]

### Changed

- Updated `AGENTS.md` and `docs/00-overview/codebase-map.md` with notes on
  `./scripts/env.sh run` TTY requirements, immutable `issue8-manifest.tsv`
  handling for non-POSIX `bin/` utilities, `vfsserv` `ro_data` files, and
  `execve()` symlink resolution.

### Fixed

- Fixed `vfsserv` `handle_read()` to fall back from `ro_data` to `data` and
  stored static `/etc/ld.so.conf` directly in `ro_data` so read-only tmpfs files
  return their text instead of NUL bytes. Added an async-signal-safe `readlink`
  fallback to Ring 3 `execve()` for symlinked executables (`/usr/bin/getent`,
  `/usr/bin/ldd`, `/usr/sbin/ldconfig`), added `$<TARGET_OBJECTS:libcso_obj>` to
  the `libc.so` CMake `DEPENDS` list for incremental rebuilds, and fixed
  `tools/posix/audit_lib.py` `_BIN_TARGET_RE` to match dynamic `bin/`
  executables in `posix.symbol_diff`.
- Routed `/boot/ld.so` `ldd` trace output and error diagnostics through generic
  `fd 1`/`fd 2` IPC dispatch (console, PTY, pipe, and VFS files) as an
  alternative to hardcoded `SYS_DEBUG_LOG`, so `ldd` output shows in SSH PTYs,
  non-interactive SSH commands, shell pipes, and file redirection. Verified via
  pipe, file, and PTY capture in `lddtest`, real SSH exec and PTY tests in
  `ipc.sshd_e2e`, and a static `SYS_DEBUG_LOG` guard in
  `arch.microkernel_boundary`.

### Added

- Added native `/usr/bin/getent` in `bin/utils/getent.c` (linked dynamically to
  `libc.so`) for `passwd`, `group`, `hosts`, `services`, and `protocols` lookups
  and full table enumeration, `/usr/bin/getent` symlink and `/etc/nsswitch.conf`
  `files` defaults in `vfsserv`, QEMU verification of `C` and `C.UTF-8`
  `setlocale`/`nl_langinfo` in both `libc.a` and `libc.so`, and a `boot.tar`
  check confirming no external `LC_*` files are packed {need:: SWR_LIBC_0038}.

- Added native `ldd` trace mode to `/boot/ld.so` (`argv[0]` ending in `ldd` or
  `LD_TRACE_LOADED_OBJECTS=1`) with `/etc/ld.so.conf` search and `/usr/bin/ldd`
  symlink {need:: SWR_LIBC_0036}.
- Added native `/usr/sbin/ldconfig` in `bin/utils/ldconfig.c` linked dynamically
  to `libc.so` to keep `DT_SONAME` symlinks, verified with `zlibtest`,
  `openssltest`, and `sshd` in QEMU `lddtest` {need:: SWR_LIBC_0037}.
- Added agent workflow guards and fast-stop QEMU diagnostics (`WDR_0013`).
  `tests/test_runner.py` now stops immediately on kernel faults, halts, and
  completed boot suites with missing patterns, with process and `llvm-addr2line`
  diagnostics. `scripts/env.sh` adds `build-code`, `test-fast`, `next-ids`,
  `task`, `check-text`, `check-ste`, and `check-fast` commands with argument
  guards. `docs/00-overview/codebase-map.md` and `AGENTS.md` add compact layout
  and command references.
- The project added the terminal runtime design for full-screen editors and
  multiplexers (`ADR_0060`), roadmap item `RDM_0013`, and tasks `TSK_0236` to
  `TSK_0243` for Vim, tmux, and Neovim over SSH and nested PTYs. It covers
  ttyserv line-discipline hardening, `fsync`, `wcwidth`, `wcswidth`, and a
  `terminfo` database.
- The CMake build makes shared `libcrypto.so.3` and `libssl.so.3` (sonames
  `libcrypto.so.3` and `libssl.so.3`) with `libcrypto.so` and `libssl.so`
  symlinks and the static archives. It installs both shared libraries into the
  Ring 3 sysroot and puts them in the boot image {need:: SWR_LIBC_0035}.
- `openssltest` links as an `ET_DYN` PIE executable to `libssl.so.3`,
  `libcrypto.so.3`, and `libc.so` with `PT_INTERP=/boot/ld.so`. It does tests of
  the `libcrypto` functions and TLS 1.3 in QEMU through `/boot/ld.so` {need::
  SWR_LIBC_0035}.
- The CMake build makes shared `libz.so.1` (soname `libz.so.1`) with a `libz.so`
  symlink and static `libz.a`. It installs `libz.so.1` into the Ring 3 sysroot
  and puts `libz.so.1` in the boot image {need:: SWR_LIBC_0034}.
- `zlibtest` links as an `ET_DYN` PIE executable to `libz.so.1` and `libc.so`
  with `PT_INTERP=/boot/ld.so`. It does tests of `zlib` in QEMU through
  `/boot/ld.so` {need:: SWR_LIBC_0034}.
- Ring 3 compiles shared `-fPIC` `libc.so` (soname `libc.so`) with static
  `libc.a` in the sysroot. The build puts `libc.so` in the boot image {need::
  SWR_LIBC_0033}.
- The new `microkernel_add_user_dynamic_executable` CMake function links
  `ET_DYN` PIE executables with `PT_INTERP=/boot/ld.so` and `DT_NEEDED=libc.so`
  through a `FILEHDR PHDRS` linker script {need:: SWR_LIBC_0033}.
- Dynamic executables send `dlopen`, `dlsym`, `dlclose`, and `dladdr` calls to
  `/boot/ld.so` through weak `ldso_*` exports in shared `libc.so` {need::
  SWR_LIBC_0033}.
- `ADR_0059` records the dynamic `libc` conventions. Those rules include image
  base 0, constructor division between `ld.so` and `__libc_start_main`, no ELF
  TLS in `libc.so`, weak exports, eager aliases, and `dlclose` rules {need::
  ADR_0059}.
- The project added roadmap item `RDM_0012` for full POSIX.1-2024 utility
  compatibility and 157 tasks (`TSK_0079` to `TSK_0235`), one for each utility
  in `tests/posix/issue8-manifest.tsv`. Each task includes command-line options,
  operands, synopsis, `stdin`, `stdout`, `stderr`, input files, output files,
  environment parameters, asynchronous events, and exit status.
- The project added workflow decision `WDR_0012` to classify new user-space
  utilities into native Ring 3 components (`/boot/ld.so`, `libc`, `bin/`) or
  out-of-tree CMake builds (`cmake/<pkg>.cmake`). It also added roadmap item
  `RDM_0011` and tasks `TSK_0075` to `TSK_0078` (`ldd`/`ldconfig`, `getent` with
  internal `C.UTF-8` locale, IANA `tzcode`, and `tar` from
  `libarchive`/`bsdtar`).
- The project added the automatic Hardware-in-the-Loop (HIL) test document and
  the Raspberry Pi 400 USB-C HIL specification in `docs/60-verification/`. It
  also added four-step QEMU-to-HIL workflow decision `WDR_0011`, initial USB
  CDC-ACM console decision `ADR_0058`, roadmap item `RDM_0010`, and tasks
  `TSK_0067` to `TSK_0074`.
- The kernel added `ET_DYN` and `PT_INTERP` executable load code with SysV
  `auxv` entries (`AT_PHDR`, `AT_PHENT`, `AT_PHNUM`, `AT_BASE`, `AT_ENTRY`). The
  project also added freestanding Ring 3 dynamic linker `/boot/ld.so`
  (`ADR_0057`, `TSK_0061`). `/boot/ld.so` applies its own relocations and loads
  `DT_NEEDED` libraries through `/dev/vfs` with `LD_LIBRARY_PATH` before
  `/boot`, `/lib`, and `/usr/lib`. It then applies `R_X86_64_*` relocations,
  obeys W^X protections, starts `DT_INIT_ARRAY` constructors, and jumps to
  `AT_ENTRY`.

### Fixed

- The project changed the `tests/stresstest/test_stress.c` boot-quiescence count
  from 8 to 9 active processes (8 resident servers such as `blkdrv` plus
  `stresstest`). It also added post-test SMP zombie-stack wait loops. As a
  result, `stresstest` waits accurately for previous boot tests in coverage
  builds with no incorrect PMM leak errors.
- The project reclassified six mislabeled option-group entries in
  `tests/posix/issue8-manifest.tsv`. It moved `encrypt` and `setkey` from `BASE`
  to `XSI` and `posix_spawnattr_{get,set}sched{param,policy}` from `BASE` to
  `SPN`. Gate 2 (`posix.symbol_diff`) writes accurate POSIX.1-2024 Base
  interface counts (`751/1041`).
- `kernel/core/cov.c` serialized LLVM coverage output (`cov_dump_proc()`,
  `cov_dump_kernel()`, and `@@COV_COMPLETE@@`) with `g_console_lock` through
  `console_lock_irqsave()` and `console_unlock_irqrestore()`. SMP `printk()` and
  `ttyserv` `FD_OP_CONSOLE_TX` output no longer mix into base64 `.profraw`
  frames or drop `[OK]` test markers during `./scripts/env.sh coverage`.
- `tests/ipccli/main.c` added retry wait loops to `/dev/echo` name lookup. SMP
  boot schedule sequence does not cause incorrect `NAME_MSG_QUERY` errors.

### Changed

- All `bin/` executables (`sh`, the XCU utilities, `nc`, `ping`, and
  `sshd-setup`) link as `ET_DYN` PIE executables with `PT_INTERP=/boot/ld.so`
  and `DT_NEEDED=libc.so`. The primary Ring 3 boot servers stay statically
  linked, and `boot.tar` decreases from 52530688 to 24249352 bytes {need::
  SWR_LIBC_0033}.
- OpenSSH Portable `sshd`, `sshd-session`, and `ssh-keygen` link as `ET_DYN` PIE
  executables with `PT_INTERP=/boot/ld.so` and `DT_NEEDED` entries for shared
  `libcrypto.so.3`, `libz.so.1`, and `libc.so`. The `openssh_libssh` and
  `openssh_compat` archives compile with `-fPIC`, and `/boot/ld.so` resolves the
  shared libraries from the boot image at load time {need:: SWR_IPC_0016}.
- The project manually changed all 150 project documents (`README.md`,
  `AGENTS.md`, `CHANGELOG.md`, and non-immutable files in `docs/`) to obey STE
  rules and use one canonical word for each concept. It also typed all `TN:` and
  `TV:` entries in `technical_terms.txt`, cleaned `allowed_phrases.txt`, and
  changed the `#traceability-documents` anchor in `docs/index.md`,
  `config/sphinx/readme_include.py`, and `tests/traceability/test_tag_scan.py`.
- The project archived completed `RDM_0003` (`70/70` Base headers, `1037/1037`
  Base interfaces, `108/108` Base XCU utilities) into `docs/records/HISTORY.md`.
  It added `RDM_0009` to `docs/records/ROADMAP.md` and six dynamic-link tasks
  (`TSK_0061` to `TSK_0066`) to `docs/records/TASKS.md`. Those tasks include
  kernel `PT_INTERP`/SysV `auxv`, Ring 3 `ld.so`, shared `libc.so`, `libz.so.1`,
  OpenSSL, OpenSSH Portable, and `bin/` utilities.
- The project added Ring 3 sysroot OpenSSL `3.0.15` (`libcrypto.a`) and `zlib
  1.3.1` (`libz.a`) to OpenSSH Portable `9.9p2` (`cmake/openssh.cmake`,
  `cmake/openssh_config.h`, `TSK_0060`). `bin/utils/sshd_setup.c` added Ed25519,
  ECDSA P-256, and RSA-2048 host key creation and `Compression delayed`. CTest
  `ipc.sshd_e2e` added tests for RSA (`aes128-gcm@openssh.com`), ECDSA P-256
  (`ecdh-sha2-nistp256`), Ed25519, and `zlib@openssh.com` compressed SSH
  sessions.
- `tools/check_ste.py`, `scripts/build-agent.sh`, and `scripts/install-hooks.sh`
  switched optional STE checks to `ste-writing-skill` (`STE_WRITING_SKILL`) with
  a clean skip when the tool is missing. The project also simplified document
  rules in `AGENTS.md`, `README.md`, `CNV_0002`, and WDRs to refer to
  `ste-writing-skill`.
- The project added three tasks in `docs/records/TASKS.md` for Ring 3 OpenSSH
  Portable. `TSK_0055` includes host-to-guest network, `netserv` changes, `nc`,
  and `ping`. `TSK_0056` includes Ring 3 PTYs, `/dev/null`, `/dev/urandom`,
  `SCM_RIGHTS`, and OpenSSH `libc` APIs. `TSK_0057` includes OpenSSH Portable
  `sshd` and `ssh-keygen`.
- The project divided `pax` from `TSK_0046` into `TSK_0054` in
  `docs/records/TASKS.md`. File diff and patch utilities (`diff`, `cmp`, `comm`,
  `patch`) and the archive utility (`pax`) complete as focused milestones.
- The project changed `docs/records/TASKS.md` to put Ring 3 TCP/IP network
  (`TSK_0041`) and the five remaining network headers (`TSK_0047`) before XCU
  utility waves. It divided `TSK_0048` into `TSK_0048` (data, event, terminal,
  and user utilities) and `TSK_0053` (batch, print, and mail utilities).
- The project changed `docs/records/TASKS.md` to put i18n headers (`TSK_0038`)
  and `<dlfcn.h>` (`TSK_0040`) before XCU utility waves to complete non-network
  Base C headers first. It also divided large utility waves into
  `TSK_0043`/`TSK_0050`, `TSK_0046`/`TSK_0051`, and `TSK_0044`/`TSK_0052`.
- The project synced `tests/posix/issue8-manifest.tsv` with the new
  `posix-issue8-os-dev` manifest. It activated option groups (`PS`, `SIO`,
  `TSA`, `RPP`) in `libc/include/sys/_posix_opts.h`, `aio.h`, `sys/mman.h`,
  `sched.h`, `pthread.h`, `sysconf.c`, and Gate 2 (`ALT_OPTION_GROUPS` for
  `FSC|SIO` and `XSI|SIO`). It also added `TSK_0049` in `docs/records/TASKS.md`
  for seven new Base interfaces (`futimens`, `utimensat`, `lgamma*`, `tzset`,
  `tzname`).
- The project changed and divided the remaining POSIX.1-2024 Base task backlog
  in `docs/records/TASKS.md`. It recorded `<stdlib.h>` multibyte, `*_l` locale
  helpers, and `wcstoimax`/`wcstoumax` in `TSK_0037`. It divided wide `stdio`
  (`TSK_0045`), diff and archive utilities (`TSK_0046`), network Base headers
  (`TSK_0047`), and remaining batch utilities (`TSK_0048`). It moved
  `<regex.h>`/`<glob.h>` (`TSK_0039`), shell grammar (`TSK_0042`), and text
  utilities (`TSK_0043`, `TSK_0046`) before i18n catalogs (`TSK_0038`) and
  network tasks.
- The project rewrote the `User-space functionality (current support)` section
  of `README.md` from a shell-user, C17-programmer, and Ring 3 server/driver
  viewpoint. That section connects to `build/posix-symbol-diff.txt` for the full
  generated symbol report.

### Added

- The CMake build added `FetchContent` compilation of OpenSSL `3.0.15` LTS into
  the Ring 3 sysroot (`sysroot/usr/lib/libcrypto.a`, `sysroot/usr/lib/libssl.a`,
  `<openssl/*.h>`). The build downloads upstream source at build time with no
  vendored files in git. `tests/openssltest/test_openssl.c` added QEMU tests for
  SHA-256, AES-256-GCM, RSA-2048, ECDSA P-256, Ed25519, X.509, and TLS 1.3
  (`TSK_0059`, `SWR_LIBC_0031`, `ADR_0056`, `COMP_LIBC_0030`, `VER_LIBC_0031`).
- The CMake build added `FetchContent` compilation of `zlib` `1.3.1` into the
  Ring 3 sysroot (`sysroot/usr/lib/libz.a`, `<zlib.h>`, `<zconf.h>`). The build
  downloads upstream source at build time with no vendored files in git.
  `tests/zlibtest/test_zlib.c` added QEMU tests (`TSK_0058`, `SWR_LIBC_0030`,
  `ADR_0055`, `COMP_LIBC_0029`, `VER_LIBC_0030`).
- The CMake build added `FetchContent` compilation of OpenSSH Portable `9.9p2`
  (`/boot/sshd`, `/boot/sshd-session`, `/boot/ssh-keygen`) with no vendored
  source in git. `/boot/sshd-setup` starts on `init=sh` interactive boot with an
  Ed25519 host key, `/mnt/etc/passwd` (dev password `microkernel`), and
  `/mnt/etc/ssh/sshd_config`. CTest `ipc.sshd_e2e` added OpenSSH client tests
  for Ed25519 public-key exec, password login, and interactive `-tt` PTY
  sessions (`TSK_0057`, `SWR_IPC_0016`, `ADR_0054`, `COMP_IPC_0021`,
  `VER_IPC_0014`).
- Ring 3 added PTYs (`/dev/ptmx`, `/dev/pts/N`, `posix_openpt`, `grantpt`,
  `unlockpt`, `ptsname*`, `<sys/ioctl.h>`, `SIGWINCH`) and virtual character
  devices (`/dev/null`, `/dev/zero`, ChaCha20 `/dev/urandom`). It also added
  Ring 3 `AF_UNIX` `SCM_RIGHTS` FD dispatch and OpenSSH `libc` APIs
  (`<syslog.h>`, `<sys/time.h>`, `<sys/uio.h>`, `crypt`, `getrusage`, `pw`/`gr`
  iterators, `setresuid`/`setresgid`, `initgroups`, `daemon`, `chroot`). This
  change added the `SIGWINCH` default-ignore rule with no new Ring 0 syscalls
  (`TSK_0056`, `SWR_IPC_0015`, `SWR_LIBC_0029`, `ADR_0053`, `ICD_IPC_0018`,
  `ICD_LIBC_0018`, `COMP_IPC_0020`, `COMP_LIBC_0028`, `VER_IPC_0013`,
  `VER_LIBC_0029`).
- `scripts/env.sh run` added localhost-only host-to-guest TCP port maps
  (`2222:22,8080:8080`). `servers/netserv/` added 32 KiB TCP RX buffers,
  window-update ACKs, passive ARP cache entries, duplicate-`SYN` replies, and TX
  virtqueue backpressure. The project also added Ring 3 `nc` and `ping`
  utilities and CTest `ipc.net_hostfwd` (`TSK_0055`, `SWR_IPC_0014`, `ADR_0052`,
  `COMP_IPC_0019`, `VER_IPC_0012`).
- Ring 3 `bin/utils/` added POSIX.1-2024 XCU `at`, `batch`, `crontab`, `lp`, and
  `mailx` (`TSK_0053`, `SWR_LIBC_0028`, `ADR_0051`, `COMP_LIBC_0027`,
  `VER_LIBC_0028`). That change brought the project to 108/108 Base XCU
  utilities, 70/70 Base C headers, and 1037/1037 Base C interfaces with QEMU
  tests in `tests/shtest/test_sh.c`.
- Ring 3 `bin/utils/` added POSIX.1-2024 XCU `uuencode`, `uudecode`, `logger`,
  `tput`, `tabs`, `mesg`, `write`, and `newgrp` (`TSK_0048`, `SWR_LIBC_0027`,
  `ADR_0050`, `COMP_LIBC_0026`, `VER_LIBC_0027`). That change raised Gate 2 from
  95/108 to 103/108 Base XCU utilities with QEMU tests in
  `tests/shtest/test_sh.c`.
- Ring 3 added POSIX.1-2024 XCU `bc` (`-l`) in `bin/bc/` and `m4` in
  `bin/utils/m4.c` (`TSK_0052`, `SWR_LIBC_0026`, `ADR_0049`, `COMP_LIBC_0025`,
  `VER_LIBC_0026`). That change raised Gate 2 from 93/108 to 95/108 Base XCU
  utilities with QEMU tests in `tests/shtest/test_sh.c`.
- Ring 3 added POSIX.1-2024 XCU `awk` in `bin/awk/` and `ed` in `bin/utils/ed.c`
  with `<regex.h>` and `<math.h>` (`TSK_0044`, `SWR_LIBC_0025`, `ADR_0048`,
  `COMP_LIBC_0024`, `VER_LIBC_0025`). That change raised Gate 2 from 91/108 to
  93/108 Base XCU utilities with QEMU tests in `tests/shtest/test_sh.c`.
- Ring 3 `bin/utils/` added POSIX.1-2024 XCU `split`, `csplit`, `fold`,
  `expand`, `unexpand`, `strings`, `tsort`, `file`, and `pr` (`TSK_0051`,
  `SWR_LIBC_0024`, `ADR_0047`, `COMP_LIBC_0023`, `VER_LIBC_0024`). That change
  raised Gate 2 from 82/108 to 91/108 Base XCU utilities with QEMU tests in
  `tests/shtest/test_sh.c`.
- Ring 3 `bin/utils/pax.c` added POSIX.1-2024 XCU `pax` (`TSK_0054`,
  `SWR_LIBC_0023`, `ADR_0046`, `COMP_LIBC_0022`, `VER_LIBC_0023`). It has read,
  write (`-w`), archive (`-r`), and copy (`-rw`) modes for `<tar.h>` `ustar` and
  `<cpio.h>` ODC formats with `-s` regex path changes. That change raised Gate 2
  from 81/108 to 82/108 Base XCU utilities.
- Ring 3 `bin/utils/` added POSIX.1-2024 XCU `diff`, `cmp`, `comm`, and `patch`
  (`TSK_0046`, `SWR_LIBC_0022`, `ADR_0045`, `COMP_LIBC_0021`, `VER_LIBC_0022`).
  That change raised Gate 2 from 77/108 to 81/108 Base XCU utilities with
  unified, standard, context, and opposite-patch tests in
  `tests/shtest/test_sh.c`.
- Ring 3 `bin/utils/` added POSIX.1-2024 XCU `sort`, `uniq`, `cut`, `tr`,
  `tail`, `tee`, `paste`, `join`, and `cksum` (`TSK_0050`, `SWR_LIBC_0021`,
  `ADR_0044`, `COMP_LIBC_0020`, `VER_LIBC_0021`). That change raised Gate 2 from
  68/108 to 77/108 Base XCU utilities with QEMU pipeline tests in
  `tests/shtest/test_sh.c`.
- Ring 3 added the remaining five POSIX.1-2024 Base network headers
  (`<netinet/in.h>`, `<netinet/tcp.h>`, `<arpa/inet.h>`, `<net/if.h>`, and
  `<netdb.h>`). It connected `AF_INET`/`AF_INET6` sockets, `sendmsg`, `recvmsg`,
  `sockatmark`, and multi-server `ppoll`/`select` through `/dev/net`
  (`TSK_0047`, `SWR_LIBC_0020`, `ADR_0043`, `ICD_LIBC_0017`, `COMP_LIBC_0019`,
  `VER_LIBC_0020`). That change reached 70/70 Base C headers and 1037/1037 Base
  C interfaces with no Ring 0 changes.
- Ring 3 added the VirtIO 1.0 PCI `virtio-net` driver and TCP/IP/loopback server
  `netserv` (`/dev/net`) with no Ring 0 syscall or module changes (`TSK_0041`,
  `SWR_IPC_0013`, `ADR_0042`, `ICD_IPC_0017`, `COMP_IPC_0018`, `VER_IPC_0011`).
  This change added `libc/include/sys/net.h` and `servers/netserv/` with MMIO
  BARs, 17-page adjacent DMA RX/TX virtqueues, and IOAPIC IRQ pulses. `netserv`
  includes loopback, Ethernet, ARP, IPv4, IPv6, ICMPv4, ICMPv6, UDP, TCP, and
  poll. It also added QEMU `-netdev user` and `virtio-net-pci` setup, plus
  `tests/nettest/test_net.c` for hardware ARP/ICMPv4 to SLIRP gateway `10.0.2.2`
  and dual-stack IPv4/IPv6 loopback ICMP/UDP/TCP tests.
- Ring 3 `bin/utils/` added POSIX.1-2024 XCU `grep`, `sed`, `find`, `xargs`,
  `expr`, and `printf` (`TSK_0043`, `SWR_LIBC_0019`, `ADR_0041`,
  `COMP_LIBC_0018`, `VER_LIBC_0019`). The utilities include BRE/ERE/fixed
  `grep`, full stream `sed` (`-E`, backrefs, hold space, `y///`, jumps), `find`
  (`-name`, `-type`, Issue 8 `-print0`, `-exec`), `xargs` (Issue 8 `-0`, `-n`,
  `-I`), `expr` (arithmetic, compare, BRE `:`), and `printf` (format loops,
  `%b`). That change raised Gate 2 to `68/108` Base utilities (+6, 63.0%) with
  QEMU tests (`shtest`) and no Ring 0 changes.
- Ring 3 `/boot/sh` completed POSIX.1-2024 XCU shell grammar and standard
  built-in commands (`TSK_0042`, `SWR_LIBC_0018`, `ADR_0040`, `COMP_LIBC_0017`,
  `VER_LIBC_0018`). The change divided `bin/sh/` into `lexer.c`, `parser.c`,
  `expand.c`, `exec.c`, `builtins.c`, and `main.c`. It added compound commands
  (`if`, `for`, `while`, `until`, `case`, subshells, `{ ... }` groups), shell
  functions, `$(...)`, `$((...))`, parameter expansions, here-documents
  (`<<`/`<<-`), `<glob.h>` path expansion, and `trap`. It also added standard
  built-in commands (`.`, `:`, `alias`, `unalias`, `command`, `eval`, `exec`,
  `getopts`, `hash`, `return`, `shift`, `times`, `break`, `continue`,
  `readonly`). That change raised Gate 2 to `62/108` Base utilities (+7, 57.4%)
  with QEMU tests (`shtest`) and no Ring 0 changes.
- Ring 3 added ELF64 `ET_DYN` dynamic load code and POSIX.1-2024 `<dlfcn.h>`
  (`TSK_0040`, `SWR_LIBC_0017`, `ADR_0039`, `ICD_LIBC_0016`, `COMP_LIBC_0016`,
  `VER_LIBC_0017`). `libc/src/dlfcn.c` added `dlopen`, `dlsym` (`RTLD_DEFAULT`,
  `RTLD_NEXT`), `dlclose`, thread-local `dlerror`, and `dladdr` (`Dl_info_t`).
  It uses adjacent W^X `mmap`/`mprotect`, `x86-64` relocations, SysV/GNU hash
  symbol tables, and `DT_INIT_ARRAY`/`DT_FINI_ARRAY` arrays. The build added
  `microkernel_add_user_shared_library` and `libdltest.so` in
  `tests/utilstest/dlplugin.c`. That change reached 100% of non-network Base
  headers (`65/70`) and `1000/1037` Base interfaces (+5, 96.4%) in Gate 2 with
  no Ring 0 changes.
- Ring 3 added POSIX.1-2024 `<langinfo.h>`, `<monetary.h>`, `<nl_types.h>`,
  `<iconv.h>`, `<libintl.h>`, and seven i18n CLI utilities (`TSK_0038`,
  `SWR_LIBC_0016`, `ADR_0038`, `ICD_LIBC_0015`, `COMP_LIBC_0015`,
  `VER_LIBC_0016`). The library added `nl_langinfo*`, `strfmon*`, `iconv`
  (`US-ASCII`/`ISO-8859-1`/`UTF-8`/`UTF-16`/`UTF-32` with `//IGNORE` and
  `//TRANSLIT`), `catopen`/`catgets`/`catclose`, and binary `.mo` `<libintl.h>`
  (`gettext*`/`ngettext*`/`*textdomain*` with `Plural-Forms`). Ring 3 also added
  `locale`, `localedef`, `iconv`, `gencat`, `msgfmt`, `gettext`, and `ngettext`.
  That change raised Gate 2 to `64/70` Base headers (+5), `995/1037` Base
  interfaces (+25), and `55/108` Base utilities (+7). QEMU (`utilstest`) and
  CTest Gates 1 and 2 completed with no Ring 0 changes.
- Ring 3 `libc` added POSIX.1-2024 `<regex.h>`, `<glob.h>`, `<wordexp.h>`,
  `<tar.h>`, and `<cpio.h>` (`TSK_0039`, `SWR_LIBC_0015`, `ADR_0037`,
  `ICD_LIBC_0014`, `COMP_LIBC_0014`, `VER_LIBC_0015`). This change added BRE/ERE
  pattern match (`regcomp`, `regexec`, `regerror`, `regfree`) in
  `libc/src/regex.c` and path match (`glob`, `globfree`) in `libc/src/glob.c`.
  It also added word expansion (`wordexp`, `wordfree`) in `libc/src/wordexp.c`
  and archive header macros in `<tar.h>` and `<cpio.h>`. It raised Gate 2 to
  `59/70` Base headers (+5) and `970/1037` Base interfaces (+8). QEMU
  (`utilstest`) and CTest Gates 1 and 2 completed with no Ring 0 changes.
- Ring 3 added wide-oriented `FILE` streams, `*wprintf`, `*wscanf`, and
  `open_wmemstream` (`TSK_0045`, `SWR_LIBC_0014`, `ADR_0036`, `ICD_LIBC_0013`,
  `COMP_LIBC_0013`, `VER_LIBC_0014`). `libc/src/stdio.c` added `fwide`,
  `open_wmemstream`, `fgetwc`/`fgetws`/`fputwc`/`fputws`/`getwc`/`getwchar`/
  `putwc`/`putwchar`/`ungetwc`, and stream `mbstate_t`. This change also added
  `fwprintf`/`wprintf`/`swprintf`/`v*wprintf` in `libc/src/wchar_printf.c` and
  `fwscanf`/`wscanf`/`swscanf`/`v*wscanf` in `libc/src/wchar_scanf.c`. It
  completed 100% of Base `<stdio.h>` and `<wchar.h>` in Gate 2 (`962/1037` Base
  interfaces, +23). QEMU (`stdiotest`) and CTest Gates 1 and 2 completed with no
  Ring 0 changes.
- Ring 3 `libc` added POSIX.1-2024 `<locale.h>`, `*_l` locale helpers,
  `<stdlib.h>` multibyte conversions, `<wctype.h>`, and primary `<wchar.h>`
  (`TSK_0037`, `SWR_LIBC_0013`, `ADR_0035`, `ICD_LIBC_0012`, `COMP_LIBC_0012`,
  `VER_LIBC_0013`). `libc/src/locale.c` added `C` and `C.UTF-8` locales
  (`setlocale`, `localeconv`, `newlocale`, `duplocale`, `freelocale`,
  `uselocale` on `FS.base` TLS, `getlocalename_l`). This change added `*_l`
  helpers in `<ctype.h>`, `<strings.h>`, `<string.h>`, and `<time.h>`, plus
  `MB_CUR_MAX` and `mblen`/`mbtowc`/`wctomb`/`mbstowcs`/`wcstombs`. It also
  added all 36 `<wctype.h>` functions in `libc/src/wctype.c` and 51 primary
  `<wchar.h>` plus `<inttypes.h>` `wcstoimax`/`wcstoumax` functions in
  `libc/src/wchar_{mb,str,conv}.c`. It raised Gate 2 to `54/70` Base headers and
  `939/1037` Base interfaces (+123). QEMU (`libctest`) and CTest Gates 1 to 3
  completed with no Ring 0 changes.
- Ring 3 `libc` and `vfsserv` added POSIX.1-2024 Base `futimens`/`utimensat`,
  `lgamma*`, and `tzset`/`tzname` (`TSK_0049`, `SWR_LIBC_0012`, `ADR_0034`,
  `ICD_LIBC_0011`, `COMP_LIBC_0011`, `VER_LIBC_0012`). This change added
  `UTIME_NOW`, `UTIME_OMIT`, `futimens`, and `utimensat` through
  `VFS_OP_UTIMENS` with isolated `st_atim`/`st_mtim`/`st_ctim` timestamps,
  `ext2_set_times()` persistence, and a changed `bin/utils/touch.c`. It also
  added Lanczos `lgamma`, `lgammaf`, and `lgammal` in `<math.h>` and
  `<tgmath.h>`, plus `tzset` and `tzname[2]` in `<time.h>` with no XSI global
  leakage. It raised Gate 2 to `816/1037` Base interfaces with QEMU (`vfstest`,
  `mathtest`, `timetest`) and CTest Gates 1 to 3 tests and no Ring 0 changes.
- Ring 3 `libc` added ISO C17 and POSIX.1-2024 `<complex.h>` and `<tgmath.h>`
  (`TSK_0036`, `SWR_LIBC_0011`, `ADR_0033`, `ICD_LIBC_0010`, `COMP_LIBC_0010`,
  `VER_LIBC_0011`). `libc/src/complex.c` added `CMPLX`/`CMPLXF`/`CMPLXL` and all
  66 `float`, `double`, and `long double` complex functions.
  `libc/src/complex_arith.c` added Annex G `__mul{s,d,x}c3` and `__div{s,d,x}c3`
  runtime helpers, and `libc/include/tgmath.h` added pure C17 `_Generic`
  dispatch. The change recorded those two headers in Gates 1 and 2 (`51/70` Base
  headers, `820/1041` Base interfaces) with QEMU tests in
  `tests/mathtest/test_math.c` and no Ring 0 changes.
- Ring 3 `libc` completed POSIX.1-2024 Base `<stdio.h>`, `<stdlib.h>`,
  `<string.h>`, and `<time.h>` (`TSK_0035`, `SWR_LIBC_0010`, `ADR_0032`,
  `ICD_LIBC_0009`, `COMP_LIBC_0009`, `VER_LIBC_0010`). `<stdio.h>` added
  `scanf*` (`libc/src/stdio_scanf.c`), `getline`/`getdelim`, `dprintf`/
  `vdprintf`, `asprintf`/`vasprintf`, float `printf` format, `fmemopen`,
  `open_memstream`, `freopen`, and `fgetpos`/`fsetpos`. `<stdlib.h>` added
  IEEE-754 `strtod`/`strtof`/`strtold`/`atof` (`libc/src/stdlib_float.c`),
  `aligned_alloc` (`libc/src/malloc.c`), `quick_exit`/`at_quick_exit`,
  `div`/`ldiv`/`lldiv`, `rand`/`srand`, `qsort_r`, and `getsubopt`. `<string.h>`
  added `stpcpy`, `stpncpy`, `memmem`, and `strerror_r`. `<time.h>` added
  `time`, `difftime`, `clock`, `timespec_get`, `gmtime*`, `localtime*`,
  `mktime`, `asctime*`, `ctime*`, and `strftime` (`libc/src/time_cal.c`). These
  changes had no Ring 0 expansion and completed `stdiotest`, `timetest`, and
  Gates 1 to 3.
- The project put the small Ring 0 microkernel boundary rule at top-level
  governance in `AGENTS.md`, `CNV_0007`, and `WDR_0010` (`SWR_DOCS_0006`,
  `VER_DOCS_0007`). It also added automatic CTest gate
  `arch.microkernel_boundary` (`tests/arch/test_microkernel_boundary.py` and
  `tests/arch/microkernel_allowlist.json`) for ADR-backed allowlists on
  `kernel/core/*.c` modules, `SYS_*` syscalls, and `struct __k_fd_entry`.
- The project recorded the headless QEMU test harness architecture, boot modes,
  and error triage model in `docs/60-verification/qemu-harness.md`. It also
  added the `qemu-debug` agent skill (`docs/skills/qemu-debug/SKILL.md`) to
  examine monitor registers, get LLVM kernel symbols, and isolate SMP races.
- The project added POSIX.1-2024 Base signals, `waitid`, `_Fork`,
  `popen`/`system`, ISO C17 `<threads.h>`, and named `MAP_SHARED` semaphores
  (`TSK_0034`, `SWR_PROC_0012`, `ADR_0031`, `ICD_LIBC_0008`, `COMP_LIBC_0008`,
  `VER_PROC_0010`). It also added XCU utilities `sleep`, `time`, `timeout`,
  `nohup`, and `sh` `wait`. `SYS_FUTEX` added physical-frame IDs on `VMM_SHARED`
  pages for Ring 3 `sem_open`/`sem_close`/`sem_unlink`. The kernel added
  `SYS_SIGTIMEDWAIT` (`64`), `SYS_SIGSUSPEND` (`65`), `SYS_SIGQUEUE` (`66`),
  `THREAD_STOPPED`, and `PROC_WAIT_*` flags. `vfsserv` added `/bin -> /boot` and
  `fexecve` lookup through `VFS_OP_RESOLVE`. Ring 3 `libc` added flat
  `siginfo_t`, `<threads.h>` on `<sys/_pthread_types.h>`, LIFO `pthread_atfork`,
  and `PTHREAD_MUTEX_ROBUST` (`EOWNERDEAD` and `ENOTRECOVERABLE`), with QEMU and
  Gates 1 to 3 tests.
- The project added POSIX.1-2024 Base VFS links, permissions, FIFOs, positional
  I/O, directory enumeration, `<sys/statvfs.h>`, temporary-file helpers, and 15
  XCU Base file utilities (`TSK_0033`, `SWR_IPC_0012`, `ADR_0030`,
  `ICD_IPC_0016`, `COMP_IPC_0017`, `VER_IPC_0010`). `vfsserv` added `VFS_OP`
  `16` to `24`, symlink lookup with `ELOOP`, `nlink` counts, 4 KiB synchronous
  FIFO buffers, `pread`/`pwrite`, `VFS_READDIR_NEXT`, and `statvfs`. Ring 3
  `libc` added 32 Base functions with no `XSI` leakage. Ring 3 added `chmod`,
  `chown`, `chgrp`, `ln`, `mkfifo`, `readlink`, `realpath`, `touch`, `rmdir`,
  `df`, `du`, `dd`, `basename`, `dirname`, and `pathchk`. These utilities
  completed `utilstest`, `shtest`, and Gates 1 to 3.
- The project added POSIX.1-2024 Base process credentials, `<pwd.h>`, `<grp.h>`,
  and `<sys/resource.h>` (`TSK_0032`, `SWR_PROC_0011`, `ADR_0029`,
  `ICD_PROC_0010`, `COMP_PROC_0010`, `VER_PROC_0009`). `pcb_t` added
  `uid`/`euid`/`suid`, `gid`/`egid`/`sgid`, supplementary groups, `nice`, and
  `rlimit` tables with `SYS_PROC_CRED` (`62`), `SYS_PROC_RLIMIT` (`63`), and
  kernel `RLIMIT_NOFILE` checks. `libc/src/cred.c` added 21 Base functions with
  no `XSI` leakage. Ring 3 added XCU utilities `id`, `logname`, `nice`, and
  `renice`, plus `sh` built-in commands `umask` and `ulimit`. `ps` changed to
  show process UIDs, GIDs, and nice values, with tests in `utilstest`, `shtest`,
  and Gates 1 to 3.
- Gate 2 (`posix.symbol_diff`, `tools/posix/audit_lib.py`,
  `tests/posix/test_symbol_diff.py`, `CMakeLists.txt`) added an index of
  POSIX.1-2024 `XCU` Base utilities. That gate makes sure that each executable
  in `bin/CMakeLists.txt` is in `boot.tar` and belongs to POSIX.1-2024 Base
  (`BDR_0004`). It also writes `utilities_base_implemented` and `## Missing
  BASE/CX/OB Utilities` into `build/posix-symbol-diff.txt`.
- The project added POSIX.1-2024 Base process inspection, configuration, and TTY
  dimension and name interfaces (`TSK_0031`, `SWR_PROC_0010`, `ADR_0028`,
  `ICD_PROC_0009`, `COMP_PROC_0009`, `VER_PROC_0008`). The kernel added
  `pcb_t.comm`, `pcb_t.args`, and read-only `FD_OP_PROC_LIST` (`ProcList()` in
  `<sys/proc.h>`). Ring 3 `libc` added `sysconf`, `confstr`, `pathconf`,
  `fpathconf`, `gethostname`, `ctermid`, `ttyname`, `ttyname_r`, `tcgetsid`,
  `tcgetwinsize`, and `tcsetwinsize` with `ttyserv`
  `TTY_OP_{GETSID,GETWINSIZE,SETWINSIZE}` and no non-POSIX names. Ring 3 also
  added XCU utilities `ps`, `getconf`, `tty`, and `stty`. Gate 2 changed to
  count standard header macros in `build/posix-symbol-diff.txt`, with tests in
  `utilstest`, `shtest`, and Gates 1 to 3.
- The project added business decision `BDR_0004`
  (`docs/10-product/bdr/BDR_0004.md`) and changed `CNV_0004` to keep public
  user-space headers, `libc`, and utilities in POSIX.1-2024. Future non-POSIX
  items must have an explicit `BDR_` or `ADR_` record. The project also added
  the dependency-ordered POSIX.1-2024 Base backlog (`TSK_0031` to `TSK_0044`) in
  `docs/records/TASKS.md`.
- The kernel added multi-core SMP boot and spinlock safety (`TSK_0025`,
  `SWR_PROC_0009`, `ADR_0027`, `COMP_PROC_0008`, `VER_PROC_0007`).
  `kernel/core/acpi.c` parses ACPI `MADT` Processor Local APIC entries. The
  kernel added portable spinlocks (`kernel/include/kernel/spinlock.h`),
  CPU-local data (`kernel/include/kernel/smp.h`, `kernel/core/smp.c`), and the
  `x86-64` 16-bit to 64-bit `INIT`/`SIPI` AP trampoline
  (`kernel/arch/x86-64/ap_trampoline.S`). It added CPU-local GDT/TSS/`GS.base`/
  `swapgs`, `x86-64-v3` `XSAVE`/`XCR0` AP initialization, Reschedule (`0xF0`)
  and synchronous TLB Shootdown (`0xF1`) IPIs, and CPU-local idle threads. The
  change added spinlocks across `sched`, `pmm`, `kheap`, `vmm`, `ipc`, `proc`,
  `time`, `signal`, `intr`, `shm`, `entropy`, `cov`, and `printk`. It also added
  `online_cpus`/`cpu_count` in `struct __k_sys_stats`/`boot_info_t`, Ring 3
  `tests/smptest/`, and 4-core CTest `proc.smp_cores`
  (`tests/boot/test_smp_cores.py`).
- Ring 3 `vfsserv` mounted a writable rev-1 `ext2` on-disk filesystem at `/mnt`
  on `/dev/blk` (`servers/vfsserv/ext2.{h,c}`, `servers/vfsserv/main.c`). It has
  automatic format and mount, level-0 and single-indirect blocks, write-through
  `open`/`read`/`write`/`ftruncate`, `mkdir`, `unlink`, `rmdir`, `rename`,
  `EXDEV`, and unlinked-open inode cleanup. This change also added Ring 3
  `tests/ext2test/` and CTest `ipc.vfs_persist`
  (`tests/boot/test_ext2_persist.py`) (`SWR_IPC_0011`, `COMP_IPC_0016`,
  `VER_IPC_0009`, `TSK_0028`).
- The kernel added `CAP_SYS_INTR`-gated `SYS_VM_MAP_MMIO`, `SYS_DMA_ALLOC`, and
  `SYS_PCI_CFG` syscalls (`kernel/core/vmm.c`, `kernel/core/intr.c`,
  `kernel/arch/x86-64/arch.c`) with Ring 3 wrappers in
  `libc/include/sys/intr.h`. The project added the `/dev/blk` IPC protocol
  (`libc/include/sys/blk.h`) and the Ring 3 VirtIO 1.0 PCI block driver
  (`servers/blkdrv/`). It also added Ring 3 `tests/blktest/` and CTest
  `ipc.blk_persist` (`tests/boot/test_blk_persist.py`) (`SWR_IPC_0010`,
  `ICD_IPC_0015`, `COMP_IPC_0015`, `VER_IPC_0008`, `TSK_0024`).
- Ring 3 `libc` added POSIX.1-2024 `<fenv.h>` (`libc/arch/x86-64/fenv.c`:
  `MXCSR` and x87 float modes and flags) and `<math.h>`
  (`libc/src/math_{basic,round,exp,trig}.c`). `<math.h>` includes IEEE-754 class
  and compare macros and the Base/CX `float`, `double`, and `long double` math
  library. It uses `x86-64-v3` hardware `FMA`, `vsqrtsd`/`vsqrtss`, and
  `vroundsd`/`vroundss`. The change recorded those two headers in Gates 1 and 2
  and added Ring 3 QEMU suite `tests/mathtest/` (`SWR_LIBC_0009`,
  `ICD_LIBC_0007`, `COMP_LIBC_0007`, `VER_LIBC_0009`, `TSK_0030`).
- The kernel added the `x86-64-v3` minimum CPU baseline check at initial boot
  (`CPUID` check stops older CPUs before `x86-64-v3` instructions operate).
  `switch.S` and `sched.c` changed to dynamic 64-byte-aligned `XSAVE`/`XRSTOR`
  context switch code with optional `AVX-512` `XCR0` checks, and `boot_info_t`
  added `BOOT_CPU_*` flags. The build compiled Ring 3 with `-march=x86-64-v3`
  and Ring 0 with scalar `x86-64-v3` integer instructions (`POPCNT`,
  `TZCNT`/`LZCNT`, `BMI1`/`BMI2`). The project changed `pmm.c` (`POPCNT` word
  reserve and adjacent scan), `kernel/lib/string.c` (64-bit scalar word
  operations), `kheap.c`, and `libc/src/string.c` (256-bit `AVX2`
  `memcpy`/`memmove`/`memset`). It also added `tests/simdtest/` and CTest
  `boot.x86_64_v3` (`SWR_BOOT_0005`, `COMP_BOOT_0005`, `VER_BOOT_0004`,
  `TSK_0029`).

### Changed

- The project recorded `ADR_0026` to set the minimum `x86-64` CPU baseline to
  `x86-64-v3` (`XSAVE`, `AVX`, `AVX2`, `FMA`, `BMI1`, `BMI2`, with optional
  `AVX-512` checks and `XSAVE`/`XRSTOR` context switch code). It added draft
  requirements `SWR_BOOT_0005` and `SWR_LIBC_0009`, and put `TSK_0029` and
  `TSK_0030` (`<math.h>` and `<fenv.h>`) before `TSK_0024`, `TSK_0028`, and
  `TSK_0025` in `docs/records/TASKS.md`.
- The project recorded the optional automatic LLVM code coverage architecture
  (`ADR_0025`, draft `SWR_DOCS_0005`) and put `TSK_0027` next in
  `docs/records/TASKS.md`. That design uses freestanding Raw Profile v8 through
  portable `arch_console_write()` base64 frames and `./scripts/env.sh coverage`
  in isolated `build-cov/`.
- The project changed `docs/records/TASKS.md` sequence by dependency (`TSK_0026`
  -> `TSK_0023` -> `TSK_0022` -> `TSK_0024` -> `TSK_0025`). It also changed
  `SWR_PROC_0008` (`waitpid(0)`/`-pgid`, `kill(-pgid)`) and `SWR_IPC_0010`
  (`CAP_SYS_INTR` MMIO/DMA syscalls).
- The project recorded the platform abstraction architecture (`ADR_0024`) and
  added `RDM_0008` in `docs/records/ROADMAP.md` for future 64-bit ARM (`arm64`)
  with no 32-bit `x86`. It added `TSK_0026` (`SWR_BOOT_0004`) to isolate
  `x86-64` code behind `x86-64` HAL directories. It also changed `README.md` and
  `docs/10-product/vision.md` to show `x86-64` as the active platform.
- `AGENTS.md` added a Mermaid document rule that tells agents to include
  vertical (`flowchart TD` / `direction TB`), timeless Mermaid diagrams where
  useful to show systems and concepts.
- The project changed `TSK_0020` (`SWR_DOCS_0004`) in `docs/records/TASKS.md` to
  add Gate 1 to 3 `<sys/select.h>` tests and PMM and server object leak checks.
  It also added `TSK_0022` (TTY job control, `ISIG`, primary utilities),
  `TSK_0023` (primary `libc` Base headers), `TSK_0024` (Ring 3 `virtio-blk`
  storage), and `TSK_0025` (ACPI `MADT` SMP boot) with draft requirements
  `SWR_PROC_0008` to `0009`, `SWR_LIBC_0008`, and `SWR_IPC_0010`.
- The project examined and changed all `POSIX` text in documents, headers, code
  comments, and audit scripts to use goal-oriented terms for `POSIX.1-2024`
  (`IEEE Std 1003.1-2024` / `ISO/IEC/IEEE 9945:2026`). It removed compliance and
  certification claims, kept `POSIX` out of titles and component names, added
  IEEE trademark notices (`README.md`, `docs/00-overview/glossary.md`,
  `docs/60-verification/traceability.md`, `docs/THIRD-PARTY-NOTICES.md`), and
  recorded these rules in `AGENTS.md`.
- The project added Mermaid architecture, sequence, and state diagrams to
  `README.md`, `docs/00-overview/context.md`, and all domain architecture pages
  (`docs/30-architecture/{boot,mem,proc,ipc,sig,libc,docs}.md`). Those diagrams
  show Ring 0/Ring 3 topology, boot steps, `x86-64` memory maps, COW page
  faults, and thread states. They also show synchronous channel IPC, IRQ pulses,
  multi-server `ppoll()`, signal frames, three-layer POSIX `libc`, and
  bidirectional traceability.
- The project added follow-up task `TSK_0021` (`SWR_BOOT_0003`) to
  `docs/records/TASKS.md` for ACPI `MADT`/`SPCR` hardware discovery and IOAPIC
  ISO dispatch. That task also includes parameterized Ring 3 `ttyserv`/`rtcdrv`
  and user `OPOST` TX ownership before `TSK_0020`.
- The project changed the backlog sequence in `docs/records/TASKS.md`
  (`TSK_0012` -> `TSK_0016` -> `TSK_0019` -> `TSK_0018` -> `TSK_0017` ->
  `TSK_0020`). It also added draft requirements `SWR_LIBC_0007` (buffered
  `<stdio.h>`, `setenv`/`unsetenv`, `execvp`, primary headers) and
  `SWR_DOCS_0004` (`<sys/select.h>` and multi-subsystem load tests).
- The project removed test counts from the `README.md` Mermaid architecture
  diagram and workflow overview. That diagram stays focused on tools and
  technologies.
- The Sphinx build included root `README.md` dynamically at the top of the
  Sphinx-Needs HTML home page (`docs/index.md` through
  `config/sphinx/readme_include.py`). That build does automatic Sphinx link
  rewrites and Mermaid diagram output.
- The project changed `README.md` to show the closed-loop AI agent development
  cycle with a Mermaid flowchart. That cycle includes Blueprint and
  bidirectional Sphinx-Needs traceability, Podman/LLVM Fabricator, headless QEMU
  Testbed, and automatic Quality Gates.
- The project changed and ordered the task backlog in `docs/records/TASKS.md`
  (`TSK_0013`, `TSK_0014`, `TSK_0015`, `TSK_0012`, `TSK_0016`, `TSK_0017`,
  `TSK_0018`) with draft requirements (`SWR_MEM_0006`, `SWR_IPC_0007` to
  `SWR_IPC_0009`, `SWR_LIBC_0006`).
- The project moved all older `agent-scaffolding` documents (`docs/TASKS.md`,
  `docs/DECISIONS.md`, `docs/CONVENTIONS.md`, `docs/VISION.md`, and
  `docs/ARCHITECTURE.md`) into the Sphinx-Needs traceability model (`WDR_0003`
  to `WDR_0009`, `ADR_0004` to `ADR_0010`, `CNV_0005` to `CNV_0006`, `RDM_0004`
  to `RDM_0007`, `docs/00-overview/context.md`, and
  `docs/10-product/vision.md`). It pointed `AGENTS.md`, `README.md`, and
  `docs/skills/` to the traceability records, kept
  `docs/THIRD-PARTY-NOTICES.md`, and removed the five older files (`TSK_0008`,
  `WDR_0009`).

### Fixed

- The project repaired a multi-core futex self-deadlock in `libc/src/malloc.c`
  (`heap_lock()`) where `__atomic_exchange_n(&g_heap_lock, 2u)` took an unlocked
  mutex (`0 -> 2`) and called `FUTEX_WAIT(2)` on its own mutex. It also reserved
  `chanid 1` for PID 1 (`sysproc`) in `kernel/core/channel.c` to prevent SMP
  boot-time channel ID races.
- The project repaired process directory (`cwd`) and file creation mask
  (`umask`) persistence across `execve()` and `fork()` in kernel `pcb_t`
  (`kernel/include/kernel/sched.h`, `kernel/core/sched.c`,
  `kernel/core/proc.c`). It added `FD_OP_GETCWD`, `FD_OP_SETCWD`, and
  `FD_OP_UMASK` through `SYS_FD_OP` (`libc/include/sys/fd.h`, `libc/src/vfs.c`).
  Utilities such as `ls` operate in the shell directory after `cd` and do not
  revert to `/`, with regression tests in `tests/shtest/test_sh.c`.
- The project repaired strict priority rules in `kernel/core/sched.c`
  (`schedule()`). A queued lower-priority thread cannot preempt an active
  higher-priority thread (such as `sysproc` at priority 110 during boot) when
  its time slice expires.

### Added

- The project added TTY job control, `ISIG` (`Ctrl-C`/`Ctrl-\`), process groups,
  sessions, and Ring 3 primary utilities (`TSK_0022`, `SWR_PROC_0008`,
  `ICD_PROC_0008`, `COMP_PROC_0007`, `VER_PROC_0006`). It added `SYS_PROC_PGID`
  (`setpgid`, `getpgid`, `getpgrp`, `setsid`, `getsid`), `SYS_KILL`
  (`kill(-pgid)`, `kill(0)`, `kill(-1)`), and `waitpid(0)`/`waitpid(-pgid)`.
  `ttyserv` added `TTY_OP_GETPGRP`, `TTY_OP_SETPGRP` (`tcgetpgrp`, `tcsetpgrp`),
  and `ISIG` `VINTR`/`VQUIT` signals. Interactive `/boot/sh` added foreground
  job groups, 8 primary utilities (`mkdir`, `rm`, `cp`, `mv`, `uname`, `env`,
  `wc`, `head`), and QEMU test `tests/jobtest/test_job.c`. The project also
  divided disk storage tasks into `TSK_0024` (`SWR_IPC_0010`) and `TSK_0028`
  (`SWR_IPC_0011`).
- Ring 3 `libc` added POSIX.1-2024 Base headers and standard C utilities
  (`TSK_0023`, `SWR_LIBC_0008`, `ICD_LIBC_0006`, `COMP_LIBC_0006`,
  `VER_LIBC_0008`). Gate 2 recorded 7 compiler freestanding headers
  (`<float.h>`, `<iso646.h>`, `<stdarg.h>`, `<stdbool.h>`, `<stddef.h>`,
  `<stdint.h>`, `<stdnoreturn.h>`). `libc` added `<sys/utsname.h>` (`uname`),
  `<endian.h>` (`htobe*`/`be*toh`/`htole*`/`le*toh`), `<sched.h>` (`sched_yield`
  through `SYS_SCHED_YIELD`), `<setjmp.h>` (`setjmp`/`longjmp` in
  `libc/arch/x86-64/setjmp.S`, `sigsetjmp`/`siglongjmp`), `<sys/times.h>`
  (`times` through `SYS_PROC_TIMES`), and `<fnmatch.h>` (`fnmatch`). It also
  added `atexit`, `qsort`, `bsearch`, `getopt`, `alarm` (`SYS_ALARM`),
  `rename`/`renameat` (`VFS_OP_RENAME`), `realpath`, and QEMU test
  `tests/utilstest/test_utils.c`. That change raised Gate 1 and Gate 2 Base
  headers from 29/70 to 42/70.
- The project added the optional automatic LLVM source code coverage pipeline
  (`TSK_0027`, `SWR_DOCS_0005`, `ADR_0025`, `ICD_DOCS_0002`, `COMP_DOCS_0004`,
  `VER_DOCS_0006`). It added `-DMICROKERNEL_COVERAGE=ON` instrumentation for
  Ring 0 and Ring 3 (`kernel/`, `libc/`, `servers/`, `bin/`, `tests/`), linker
  `__llvm_prf_*` symbols, and freestanding Raw Profile v8 output through
  `arch_console_write()` (`SYS_COV_WRITE`). It also added `tests/test_runner.py
  --cov-out --wait-cov`, `tools/coverage/generate_report.py`, CTest
  `coverage.unit`, `./scripts/env.sh coverage` (`build-cov/coverage/html/
  index.html`, `lcov.info`, `summary.txt`), and `build-cov/` in `.gitignore`.
- The project isolated the `x86-64` Hardware Abstraction Layer (HAL) and added a
  parameterized platform build (`TSK_0026`, `SWR_BOOT_0004`, `ADR_0024`,
  `ICD_BOOT_0002`, `COMP_BOOT_0004`, `VER_BOOT_0003`). It moved `x86-64` kernel,
  linker, `libc` CRT/syscall/TLS/port-I/O, and Ring 3 UART/RTC hardware code
  into `x86-64` directories (`kernel/arch/x86-64/`, `libc/arch/x86-64/`,
  `servers/ttyserv/arch/x86-64/`, `servers/rtcdrv/arch/x86-64/`,
  `cmake/arch/x86-64.cmake`). It added portable `kernel/include/kernel/arch.h`
  and trapframe accessors, and configured CMake (`-DMICROKERNEL_ARCH=x86-64`),
  `scripts/env.sh`, `tests/test_runner.py`, and POSIX check gates (`--target`
  and `AArch64` relocation regexes).
- The project added `<sys/select.h>` (`select`, `pselect`, `fd_set`, `FD_*`),
  kernel zombie cleanup, and a multi-subsystem load test suite (`TSK_0020`,
  `SWR_DOCS_0004`, `ADR_0023`, `ICD_IPC_0014`, `COMP_IPC_0014`,
  `VER_DOCS_0005`). It added standalone `libc/include/sys/select.h`
  (`suseconds_t`, `struct timeval`, `FD_SETSIZE`) and `libc/src/select.c` on
  multi-server `ppoll()` with cancellation points, plus Gate 1 to Gate 3 checks
  in `posix.headers`, `posix.symbol_diff`, and `posix.cancellation`. The kernel
  added zombie process and thread stack/TCB/FPU cleanup (`proc_reap_zombie()`,
  `proc_reap_zombie_threads()`, `proc_user_stack_alloc()`), `SYS_SYS_STATS`
  (`52`), `PIPE_OP_STATS`/`VFS_OP_STATS`/`SOCK_OP_STATS` RPCs, and QEMU
  `tests/stresstest/test_stress.c`.
- The project added ACPI `MADT`/`SPCR` hardware discovery, parameterized Ring 3
  drivers, Ring 3 `OPOST`/`ECHO` TX, and interactive QEMU shell boot
  (`SWR_BOOT_0003`, `ADR_0022`, `COMP_BOOT_0003`, `VER_BOOT_0002`).
  `kernel/core/acpi.c` parses RSDP/RSDT/XSDT, `MADT` IOAPIC base and ISOs, and
  optional `SPCR`, and `kernel/core/intr.c` added ISO IOAPIC setup.
  `kernel/core/proc.c` added `spawn_image_argv()` and `init=sh` interactive boot
  mode, and `servers/ttyserv/main.c` and `servers/rtcdrv/main.c` added `-p`/`-i`
  arguments. `libc/src/tty.c` and `libc/src/fd.c` added Ring 3 16550A
  `OPOST`/`ONLCR`/`ECHO` TX dispatch through `/dev/tty`, with `./scripts/env.sh
  run` and instructions in `README.md`.
- Ring 3 added command shell `sh` and primary utilities for `POSIX.1-2024` XCU
  (`SWR_LIBC_0006`, `ADR_0021`, `COMP_LIBC_0005`, `VER_LIBC_0007`).
  `bin/sh/main.c` added `-c`, script files, interactive `isatty()` mode,
  pipelines on `pipe2`/`fork`/`dup2`/`execvp`, and redirections
  `<`/`>`/`>>`/`2>`/`2>&1`. It also added `&&`/`||`/`;`, `$VAR`/`$?`, Issue 8
  `$'...'` ANSI-C syntax, `set -o pipefail`, `read -d`, and built-in commands
  `cd`/`pwd`/`export`/`unset`/`exit`. The project added standalone
  `bin/utils/{echo,cat,ls,true,false,test}.c`, `bin` checks in format and lint
  scripts, and QEMU `tests/shtest/test_sh.c`.
- Ring 3 added the `AF_UNIX` local sockets server and POSIX.1-2024 `accept4`
  (`SWR_IPC_0009`, `ADR_0020`, `ICD_IPC_0012` to `0013`, `COMP_IPC_0012` to
  `0013`, `VER_IPC_0007`). `servers/sockserv/main.c` (`/dev/sock`) gives
  `AF_UNIX` `SOCK_STREAM` and `SOCK_DGRAM` sockets with deferred synchronous
  `accept`/`send`/`recv`/`ppoll` queues, and `kernel/core/proc.c` added
  `FD_KIND_SOCK` close pulses. Ring 3 added `<sys/socket.h>`, `<sys/un.h>`,
  async-signal-safe `libc/src/socket.c` (`socket`, `socketpair`, `bind`,
  `listen`, `accept`, `accept4` with `SOCK_CLOEXEC`/`SOCK_NONBLOCK`, `connect`,
  `send`, `recv`, `sendto`, `recvfrom`, `shutdown`, `getsockname`,
  `getpeername`, `getsockopt`, `setsockopt`), multi-server `ppoll()` dispatch in
  `libc/src/fd.c`, and QEMU `tests/socktest/test_sock.c`.
- Ring 3 `libc` added buffered POSIX.1-2024 `<stdio.h>`, environment functions,
  `execvp`, and primary C17/POSIX headers (`SWR_LIBC_0007`, `ADR_0019`,
  `ICD_LIBC_0005`, `COMP_LIBC_0004`, `VER_LIBC_0006`). It added standalone
  `<ctype.h>`, `<assert.h>`, `<inttypes.h>`, `<strings.h>`, and `<limits.h>`
  with C17 and POSIX runtime limits. It also added buffered `FILE` streams
  (`fopen`, `fdopen`, `fclose`, `fread`, `fwrite`, `fgets`, `fgetc`, `ungetc`,
  `fseeko`, `ftello`, `fileno`, `setvbuf`, `vsnprintf`, `vfprintf`, `perror`),
  `setenv`/`unsetenv`, `execv`/`execvp` with `/boot/` `bootfs` lookup,
  `strdup`/`strndup`/`strstr`/`strrchr`/`memchr`/`strtok_r`/`strerror`, and QEMU
  `tests/stdiotest/test_stdio.c`.
- Ring 3 added the serial TTY server and multi-server `ppoll()` (`SWR_IPC_0008`,
  `ADR_0018`, `ICD_IPC_0010` to `0011`, `COMP_IPC_0010` to `0011`,
  `VER_IPC_0006`). `servers/ttyserv/main.c` (`/dev/tty`) uses COM1 IRQ 4 IOAPIC
  pulses with canonical (`ICANON`, `VERASE`, `VKILL`, `ICRNL`, `VEOF`) and raw
  (`VMIN`) line discipline. Ring 3 added standalone `<termios.h>`, `isatty`,
  async-signal-safe `libc/src/tty.c` (`tcgetattr`, `tcsetattr`, `tcflush`,
  `tcdrain`, `tcflow`, `tcsendbreak`, `cf*speed`), multi-server `ppoll()`
  dispatch in `libc/src/fd.c`, and QEMU `tests/ttytest/test_tty.c`.
- The project added option-gated POSIX.1-2024 memory extensions (`SWR_MEM_0005`,
  `ADR_0017`, `COMP_MEM_0005`, `VER_MEM_0004`). `libc/include/sys/_posix_opts.h`
  added `_POSIX_SHARED_MEMORY_OBJECTS`, `_POSIX_MEMLOCK`, and
  `_POSIX_MEMLOCK_RANGE` (`202405L`). The kernel added shared-frame
  `MAP_SHARED`, `SYS_SHM_OP`, `SYS_SHM_MAP`, `FD_KIND_SHM`
  (`kernel/core/shm.c`), `VMM_SHARED`, `VMM_LOCKED`, and `SYS_VM_MLOCK`
  (`kernel/core/vmm.c`). `libc/src/mman.c` added `shm_open`, `shm_unlink`,
  `mlock`, `munlock`, `mlockall`, and `munlockall`, `libc/src/vfs.c` added
  `ftruncate`/`fstat` for SHM, and `tests/shmtest/test_shm.c` added QEMU tests.
- Ring 3 added the VFS server (`SWR_IPC_0007`, `ADR_0016`, `ICD_IPC_0007` to
  `0009`, `COMP_IPC_0008` to `0009`, `VER_IPC_0005`). `servers/vfsserv/main.c`
  (`/dev/vfs`) gives read-only `boot.tar` at `/boot` and writable dynamic
  `tmpfs` at `/` and `/tmp`, and `kernel/core/proc.c` added read-only
  `BOOTFS_USER_VA` maps and `FD_KIND_VFS` close pulses. Ring 3 added
  `<sys/stat.h>`, `<dirent.h>`, `libc/src/vfs.c` (`open`, `openat`, `creat`,
  `lseek`, `ftruncate`, `stat`, `fstat`, `lstat`, `fstatat`, `mkdir`, `mkdirat`,
  `unlink`, `unlinkat`, `rmdir`, `chdir`, `fchdir`, `getcwd`, `umask`, `access`,
  `opendir`, `fdopendir`, `readdir`, `rewinddir`, `closedir`, `dirfd`), and QEMU
  `tests/vfstest/test_vfs.c`.
- Ring 3 `libc` added a thread-safe `mmap`/`munmap` heap allocator
  (`SWR_MEM_0006`, `ADR_0015`, `COMP_MEM_0004`, `VER_MEM_0003`).
  `libc/src/malloc.c` replaced the static 1 MiB linear arena in
  `libc/src/stdlib.c` with a `SYS_FUTEX` mutex and 64 KiB slabs with full-slab
  `munmap` release. It also added isolated `mmap`/`munmap` calls for allocations
  >= 32 KiB, local `realloc` growth and division, POSIX.1-2024 `errno`-safe
  `free`, and QEMU `tests/heaptest/test_heap.c`.
- The project added the full 4-gate POSIX.1-2024 compatibility test harness
  (`SWR_DOCS_0003`, `COMP_DOCS_0003`, `VER_DOCS_0004`). It added CTest Gate 2
  `posix.symbol_diff` (`tests/posix/test_symbol_diff.py` compares Clang JSON AST
  declarations and `llvm-nm` `libc.a` exports with
  `tests/posix/issue8-manifest.tsv`) and Gate 3b `posix.cancellation`
  (`tests/posix/test_cancellation.py` examines the XSH 2.9.5 cancellation call
  graph). It also added `realloc*`, `<stdio.h>` stream locks, and mutex locks to
  `posix.async_signal_safe` unpermitted callees, and put call graph and manifest
  helpers in `tools/posix/audit_lib.py`.
- Ring 3 `libc` added POSIX.1-2024 Base library functions and C17 headers
  (`SWR_LIBC_0005`, `ADR_0014`). This change added kernel `SYS_GETENTROPY`
  (`48`) mixer (`kernel/core/entropy.c`), `getentropy` (`GETENTROPY_MAX = 256`),
  `strlcpy`, `strlcat`, `realloc`, `reallocarray`, `_Exit`, `getenv`, and
  `secure_getenv`. It also added `<uchar.h>` UTF-16/UTF-32 conversions
  (`libc/src/uchar.c`), standalone `<stdatomic.h>`/`<stdalign.h>` checks, and
  QEMU `tests/libctest/test_libc_issue8.c`.
- Ring 3 `libc` added POSIX.1-2024 Base thread cancellation points and recursive
  `<stdio.h>` stream mutexes (`SWR_LIBC_0004`, `ADR_0013`). It added
  `pthread_cancel`, `pthread_setcancelstate`, `pthread_setcanceltype`,
  `pthread_testcancel`, `pthread_cleanup_push`/`pop`, and internal
  `LIBC_SIGCANCEL` wakeup across `pthread_cond_wait`, `read`, `write`, `close`,
  `ppoll`, `sem_wait`, `clock_nanosleep`, `waitpid`, `aio_suspend`, and
  `pthread_join`. It also added `FILE` (`stdin`/`stdout`/`stderr`), `flockfile`,
  `ftrylockfile`, `funlockfile`, `fprintf`, `fputs`, `fputc`,
  `putchar_unlocked`, `fflush`, and QEMU `tests/canceltest`.
- Ring 3 `libc` added POSIX.1-2024 Base asynchronous I/O (`SWR_IPC_0005`,
  `ADR_0012`). It added `<aio.h>`, `O_SYNC`/`O_DSYNC` in `<fcntl.h>`, and the
  Ring 3 worker-thread engine (`libc/src/aio.c`) with `aio_read`, `aio_write`,
  `aio_fsync`, `aio_cancel`, `lio_listio`, and `SIGEV_SIGNAL`. It also added
  async-signal-safe `aio_error`/`aio_return`/`aio_suspend`, QEMU
  `tests/aiotest`, and CTest header and safety checks.
- The project added POSIX.1-2024 Base `fork`, `execve`, `pipe2`, `dup3`,
  `fcntl`, `poll`, `ppoll`, and `sigpending` (`TSK_0007`, `SWR_PROC_0004`,
  `SWR_IPC_0004`, `ADR_0011`). The kernel added syscalls `SYS_PROC_FORK` (`42`),
  `SYS_PROC_EXECVE` (`43`), `SYS_PROC_GETPID` (`44`), `SYS_PROC_GETPPID` (`45`),
  `SYS_FD_OP` (`46`), `SYS_SIGPENDING` (`47`), and process FD metadata with
  `PULSE_CODE_FD_CLOSE` cleanup. Ring 3 added `servers/pipeserv` (`/dev/pipe`),
  `<fcntl.h>`, `<poll.h>`, async-signal-safe `libc/src/fd.c`, QEMU
  `tests/forkpipetest`/`exechelper`, and CTest checks.
- The project added POSIX.1-2024 Base `<pthread.h>`, `<semaphore.h>`, and
  hardware `x86-64` `FS.base` TLS (`TSK_0006`, `SWR_PROC_0005`, `ADR-19`). The
  kernel added syscalls `SYS_THREAD_SET_FSBASE` (`39`), `SYS_THREAD_EXIT`
  (`40`), and `SYS_FUTEX` (`41`). Ring 3 `libc` added O(1) TLS `errno`, mutexes,
  `pthread_cond_clockwait`, barriers, spinlocks, rwlocks, TSD slots,
  async-signal-safe `sem_post`, QEMU `tests/pthreadtest`, and CTest checks.
- The project added POSIX.1-2024 Base monotonic and realtime clocks and interval
  timers (`TSK_0005`, `SWR_PROC_0006`, `ADR-18`). It added syscalls `30` to
  `38`, `kernel/core/time.c`, `<time.h>`, async-signal-safe `libc/src/time.c`,
  `sleep()` on `clock_nanosleep`, QEMU `tests/timetest`, and
  `posix.headers`/`posix.async_signal_safe` checks.
- The project added the unified pre-commit quality gate (`tools/check_ste.py`,
  `scripts/build-agent.sh`, and `scripts/install-hooks.sh`, `ADR-17`). That gate
  runs full-repo `clang-format`, `docs-traceability-setup` `check_text.py --root
  .`, optional `ste-writing-skill`, build, QEMU tests, `clang-tidy`, and CTest
  before each commit.
- The kernel added Milestone 6 (`TSK_0004`) hardware interrupt, port I/O, and
  multi-buffer IPC syscalls. Those syscalls include `SYS_INTR_ATTACH_EVENT`
  (`11`), `SYS_INTR_UNMASK` (`12`), `SYS_IOPL_SET` (`25`, `CAP_SYS_INTR`-gated),
  `SYS_MSG_READV` (`8`), and `SYS_MSG_WRITEV` (`9`) with IOAPIC dispatch
  (`kernel/core/intr.c`) and pre-allocated IRQ IPC pulses.
- Ring 3 added `<sys/intr.h>` (`AttachIntrEvent`, `IntrUnmask`, `IOPLSet`,
  `port_in*`/`port_out*`) and `ReadMsgV`/`WriteMsgV`/`ipc_iov_t` in
  `<sys/ipc.h>`. It also added the Ring 3 MC146818 CMOS RTC driver
  (`servers/rtcdrv`, `/dev/rtc`) and unprivileged client test (`tests/drvtest`).
- The kernel added Milestone 5 (`TSK_0002`) process and memory syscalls:
  `SYS_PROC_CREATE`, `SYS_PROC_WAIT` (`28`), `SYS_VM_MAP`, `SYS_VM_UNMAP`,
  `SYS_VM_CLONE_COW`, and `SYS_VM_PROTECT` (`29`). This change added PMM frame
  counts, Ring 3 `#PF` COW page-fault service, and user PML4 release on child
  reap.
- Ring 3 added POSIX.1-2024 `<sys/mman.h>` (`mmap`, `munmap`, `mprotect`,
  `msync`) and `<sys/wait.h>` (async-signal-safe `wait`, `waitpid`, `W*`
  macros). It also added native `<sys/proc.h>` and `<sys/vm.h>` headers and
  `libc` wrappers.
- The project added automatic Milestone 5 tests (`tests/proctest/main.c` and
  `child.c`) that operate at the same time as Milestones 1 to 4 in QEMU. It also
  added `<sys/mman.h>` and `<sys/wait.h>` checks in `posix.headers` and
  `wait`/`waitpid` checks in `posix.async_signal_safe`.
- Ring 3 added POSIX.1-2024 `<signal.h>` (`libc/include/signal.h`) and
  async-signal-safe `libc/src/signal.c` (`sigaction`, `sigprocmask`,
  `pthread_sigmask`, `kill`, `pthread_kill`, `raise`, `signal`, and `sigset_t`
  helpers). The kernel added `SYS_SIGPROCMASK` (`27`) and kept and loaded
  `sig_frame_t.saved_mask` across `SYS_SIGRETURN`.
- The project added Gate 3 CTest `posix.async_signal_safe`
  (`tests/posix/test_async_signal_safe.py`) that examines
  `sysroot/usr/lib/libc.a` with `llvm-objdump -dr`. It also added `<signal.h>`
  standalone checks in `posix.headers` and QEMU runtime checks in
  `tests/sigtest/main.c`.
- The project added two-layer bidirectional code and POSIX.1-2024 traceability.
  This change added `:posix:` fields and a status matrix in
  `docs/60-verification/traceability.md`, plus bidirectional `ICD_`/`COMP_`/
  `VER_` checks in `config/sphinx/need_tag_check.py`. It also added `WDR_0002`,
  `ICD_`/`COMP_` tags across `kernel/`, `libc/`, `servers/`, and traceability
  tools, and rules in `AGENTS.md`.
- Ring 3 `libc` added `<errno.h>` and lock-free TID-indexed thread-local `errno`
  (`libc/src/errno.c`) with the target POSIX.1-2024 `E*` constants in
  `libc/include/sys/errno.h`.
- The project added the standalone POSIX.1-2024 header compilation and namespace
  hygiene CTest gate (`posix.headers` in `tests/posix/test_headers.py`) and
  multithreaded `errno` tests in `tests/sigtest/main.c`.
- The project added POSIX.1-2024 (Issue 8) Base and option-group software
  requirements across `LIBC`, `SIG`, `MEM`, `PROC`, `IPC`, and `DOCS`. Those
  requirements include canonical `9799919799` specification URLs, `ADR_0003`,
  `CNV_0004`, and `TSK_0003`.
- The project added traceability domains (`BOOT`, `MEM`, `PROC`, `IPC`, `SIG`,
  `LIBC`, and `DOCS`) across all Sphinx-Needs layers. It also added `BDR_0002`,
  `BDR_0003` (POSIX.1-2024 Base compatibility target), `ADR_0002`, and
  `microkernel` task and roadmap records.
- The project added the docs-as-code traceability model with the
  `docs-traceability-setup` agent skill 0.1.0. The configuration is in
  `.docs-traceability-setup.yml`.
- The project added Sphinx documents in `docs/` with MyST-Parser and
  Sphinx-Needs. CMake compiles the documents with warnings as errors, and CTest
  does unit tests, HTML build, JSON export, and model checks.
- The project added traceability example items for the `POWER` domain, decision
  records, and project records in `docs/records/`.
- The project added `.editorconfig` with file header notice lines and the Python
  dependency manifest `requirements.txt`.

### Changed

- `CMakeLists.txt` added `${CMAKE_SOURCE_DIR}/kernel` to `kernel.elf` include
  directories and replaced all parent-relative `../arch/x86_64/` includes with
  `<arch/x86_64/...>` across the kernel.
- `scripts/install-hooks.sh` changed `.git/hooks/pre-commit` to do full CTest
  (`traceability.unit`, `docs.html`, `docs.needs`, `docs.audit`,
  `posix.headers`, `posix.async_signal_safe`) with new `build/docs/doctrees`.
- `docs/00-overview/glossary.md` added entries for Ring 0, Ring 3, syscalls,
  PCB/TCB, IPC, PMM/VMM, PML4, SMEP/SMAP, COW, `bootfs`, sysroot, and
  async-signal-safety.
- `docs/TASKS.md` and `docs/records/TASKS.md` added open tasks (`TSK_0004` to
  `TSK_0007`) for Milestone 6 and the next POSIX.1-2024 Base components. The
  project also archived completed `RDM_0001` into `docs/records/HISTORY.md`.
- `README.md` added a user-space function table, active user-space examples, and
  an overview of bidirectional Sphinx-Needs traceability. `AGENTS.md` added a
  rule that tells agents to keep the `README.md` function summary accurate.
- `README.md` added the POSIX.1-2024 (Issue 8) Base compatibility target, CTest
  commands, and links to `BDR_0003`, `ADR_0003`, and the traceability matrix.
- The project aligned `.clang-format` (`ColumnLimit: 80`) with `.editorconfig`
  and added `.editorconfig` exemptions for `LICENSE.md` and
  `docs/skills/*/references/*`. It changed all C/H, CMake, shell, and Python
  files to Doxygen headers and wrapped documents. As a result, full-repo
  `check_text.py --root .` completes with no errors.
- `AGENTS.md`, `WDR_0002`, and `CNV_0002` made `.editorconfig` and
  `docs-traceability-setup` `check_text.py` checks (line width, greedy prose
  format, and Doxygen file headers) mandatory, and `AGENTS.md` wrapped to 80
  columns.
- `.docs-traceability-setup.yml` changed `authoring.modal_verb` from `shall` to
  `must`. The project changed all requirements in `docs/20-system/` with
  `ste-writing-skill`, expanded `technical_terms.txt` and `allowed_phrases.txt`,
  and recorded STE rules in `AGENTS.md`, `WDR_0002`, and `CNV_0002`.
- The project changed in-code `VER_` tags in `tests/test_runner.py`,
  `tools/traceability/check_needs_audit.py`, and `tests/traceability/` from
  `VER_POWER_*` to `BOOT`, `MEM`, `PROC`, `IPC`, `SIG`, `LIBC`, and `DOCS`
  verification items.
- The root `CMakeLists.txt` compiles the documents with `add_subdirectory(docs)`
  and activates CTest.
- The build container adds the `python3-venv` and `python3-pip` packages for the
  document venv.
- The headless QEMU test harness `tests/test_runner.py` has the `VER_POWER_0005`
  traceability tag.

### Removed

- The project removed the `POWER` placeholder domain documents
  (`docs/*/power.md`) from the traceability model.

### Fixed

- The kernel repaired the `schedule()` loop in `kernel/core/sched.c` to activate
  interrupts (`sti; hlt; cli`) and poll `pick_next()` again. It also removes
  woken self-threads correctly and prevents recursive `sched_tick()` preemption
  when all threads block.
- `pmm_init()` reserved Multiboot2 boot modules (`bootfs`). Dynamic frame
  allocations do not overwrite the boot archive that `ProcCreate` uses.
- `ISR_STUB` in `kernel/arch/x86_64/isr.S` removed all 16 bytes (`vec` and
  `err`) before `iretq` on error-code CPU faults such as recoverable `#PF` COW
  page faults.
- `sys_thread_reset` aligned secondary Ring 3 thread entry stacks to obey the
  System V AMD64 `RSP % 16 == 8` C function entry rule. That change prevents
  `#GP` faults on SSE `movaps` instructions in new worker threads.

### Added

- The project added the scaffold from the `agent-scaffolding` agent skill 0.2.0.
  The configuration is in `.agent-scaffolding.yml`.
- The project added the MIT License in `LICENSE.md`, with copyright headers and
  SPDX identifiers in the source files.
- The project added governance and state documents: `AGENTS.md`,
  `docs/VISION.md`, `docs/DECISIONS.md`, `docs/CONVENTIONS.md`,
  `docs/THIRD-PARTY-NOTICES.md`, and this changelog.
- The project added agent skills in `docs/skills/`, with the `Conventional
  Commits`, `SemVer`, and `Agent Skills` specifications.
- The project added containerized development environment wrapper
  `scripts/enter-dev.sh`, plus build and format agent scripts with their agent
  definitions in `docs/agents/`.
- The project added Clang tool configurations `.clang-format` and `.clang-tidy`,
  with tests in the build container.

### Changed

- The task plan moved from `TASKS.md` at the repository root to `docs/TASKS.md`.
- The test harness and test executables moved to `tests/`:
  `tests/test_runner.py`, `tests/ipccli/`, and `tests/sigtest/`.
