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

### TSK_0045 — Add wide-oriented <stdio.h>/<wchar.h> I/O and open_wmemstream

```{task} Add wide-oriented <stdio.h>/<wchar.h> I/O and open_wmemstream
:id: TSK_0045
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001

Extend Ring 3 `FILE` streams with wide orientation (`fwide`) and wide character
I/O (`fgetwc`, `fgetws`, `fputwc`, `fputws`, `getwc`, `getwchar`, `putwc`,
`putwchar`, `ungetwc`). Add wide formatted I/O (`fwprintf`, `wprintf`,
`swprintf`, `vfwprintf`, `vwprintf`, `vswprintf`, `fwscanf`, `wscanf`,
`swscanf`, `vfwscanf`, `vwscanf`, `vswscanf`) and `open_wmemstream` with QEMU
tests ({need}`BDR_0004`, {need}`SYS_LIBC_0001`).
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

### TSK_0042 — Complete POSIX.1-2024 XCU shell grammar and built-ins

```{task} Complete POSIX.1-2024 XCU shell grammar and built-ins
:id: TSK_0042
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001

Extend `/boot/sh` with compound commands (`if`, `for`, `while`, `until`, `case`,
subshells), functions, `$(...)` command substitution, `$((...))` arithmetic,
parameter expansions, here-documents, pathname globbing via `<glob.h>`, and
`trap`. Add remaining standard built-ins (`.`, `:`, `alias`, `unalias`,
`command`, `eval`, `exec`, `getopts`, `hash`, `return`, `shift`, `times`)
({need}`BDR_0004`, {need}`SYS_LIBC_0001`).
```

### TSK_0043 — Add POSIX.1-2024 XCU search, filter, and sort text utilities

```{task} Add POSIX.1-2024 XCU search, filter, and sort text utilities
:id: TSK_0043
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001

Add search and stream utilities `grep`, `sed`, `find`, `xargs`, `sort`, `uniq`,
`cut`, `tr`, `tail`, and `tee`. Also add `paste`, `join`, `printf`, `expr`, and
`cksum` with QEMU tests ({need}`BDR_0004`, {need}`SYS_LIBC_0001`).
```

### TSK_0046 — Add POSIX.1-2024 XCU diff, archive, and formatting utilities

```{task} Add POSIX.1-2024 XCU diff, archive, and formatting utilities
:id: TSK_0046
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001

Add comparison and patching utilities `diff`, `cmp`, `comm`, and `patch`. Add
archive and text-formatting utilities `pax`, `split`, `csplit`, `fold`,
`expand`, `unexpand`, `strings`, `tsort`, `file`, and `pr` with QEMU tests
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

### TSK_0040 — Add Ring 3 ELF dynamic symbol resolution and <dlfcn.h>

```{task} Add Ring 3 ELF dynamic symbol resolution and <dlfcn.h>
:id: TSK_0040
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001, SYS_PROC_0001

Add Ring 3 ELF dynamic object loading (`ET_DYN` with W^X `mmap`/`mprotect`
relocation) for `<dlfcn.h>` (`dlopen`, `dlsym`, `dlclose`, `dlerror`, `dladdr`).
Verify with Gates 1 and 2 and QEMU runtime tests ({need}`BDR_0004`,
{need}`SYS_LIBC_0001`).
```

### TSK_0041 — Add Ring 3 PCI virtio-net driver and TCP/IP/loopback server

```{task} Add Ring 3 PCI virtio-net driver and TCP/IP/loopback server
:id: TSK_0041
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_IPC_0001, SYS_LIBC_0001

Add a Ring 3 PCI `virtio-net` driver using existing `CAP_SYS_INTR` MMIO, DMA,
and IOAPIC IRQ pulses. Add a Ring 3 network server with loopback, Ethernet, ARP,
IPv4, IPv6, ICMP, UDP, and TCP, verified in QEMU ({need}`BDR_0004`,
{need}`SYS_IPC_0001`).
```

### TSK_0047 — Add 5 network Base headers and socket/resolver libc APIs

```{task} Add 5 network Base headers and socket/resolver libc APIs
:id: TSK_0047
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_IPC_0001, SYS_LIBC_0001

Add `sendmsg`, `recvmsg`, and `sockatmark` over Ring 3 socket servers. Supply
`<netinet/in.h>`, `<netinet/tcp.h>`, `<arpa/inet.h>`, `<netdb.h>`, and
`<net/if.h>` (`htons`/`htonl`/`ntohs`/`ntohl`, `inet_*`, `if_*`, `getaddrinfo`,
`getnameinfo`, `gai_strerror`, `get*ent`) to reach 70 Base headers
({need}`BDR_0004`, {need}`SYS_IPC_0001`, {need}`SYS_LIBC_0001`).
```

### TSK_0044 — Add POSIX.1-2024 XCU language and editor utilities

```{task} Add POSIX.1-2024 XCU language and editor utilities
:id: TSK_0044
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001

Add `awk`, `bc`, `ed`, and `m4` over `<regex.h>` and `<math.h>` with QEMU
runtime tests ({need}`BDR_0004`, {need}`SYS_LIBC_0001`).
```

### TSK_0048 — Add remaining XCU encoding, terminal, and batch utilities

```{task} Add remaining XCU encoding, terminal, and batch utilities
:id: TSK_0048
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001

Add `uuencode`, `uudecode`, `logger`, `tput`, `tabs`, `mesg`, `write`, and
`newgrp`. Also add `at`, `batch`, `crontab`, `lp`, and `mailx` to complete the
remaining XCU Base utility set ({need}`BDR_0004`, {need}`SYS_LIBC_0001`).
```
