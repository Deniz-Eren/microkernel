## Purpose

Record the third-party open-source software, files, snippets, and libraries that
`microkernel` uses. Give the license attribution and the compliance notices.

## Structure

Make one section for each third-party component. Each section gives the source,
the license, and the function of the component in the project.

## Format Rules

- Give the license text or a link to it.
- Keep these entries accurate. Add an entry when you add a third-party
  component.

---

This document records the third-party open-source software, files, snippets, and
libraries that this project uses. We give thanks to the open-source community
for its work.

The notices, terms, and copyright lines that follow give the attribution for
their open-source licenses. These licenses apply only to the third-party files
and dependencies in this document. They do not change the primary license of
this project. The primary license is in [LICENSE.md](../LICENSE.md).

## 1. Copied files and snippets

### conventionalcommits.org
* **Component(s) Used:** [COMMITS.md](skills/git/references/COMMITS.md)
* **Source:** [conventionalcommits.org (v1.0.0)][cc-src]
* **Source License:** MIT License

#### License Notice

```text
MIT License

Copyright (c) 2018 Conventional Changelog

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### agentskills
* **Component(s) Used:** [SKILLS.mdx](skills/create-skill/references/SKILLS.mdx)
* **Source:** [github.com/agentskills/agentskills][agentskills-src]
* **Source License:** Apache License 2.0. The full license text is in the
  agentskills repository at the source link above.

### Semantic Versioning 2.0.0
* **Component(s) Used:** [semver.md](skills/git/references/semver.md)
* **Source:** [github.com/semver/semver.md][semver-src]
* **Source License:** Creative Commons Attribution 3.0 Unported (CC BY 3.0). The
  full license text is at [creativecommons.org/licenses/by/3.0][cc-by-3].

### keepachangelog.com
* **Component(s) Used:**
  [skills/keepachangelog/SKILL.md](skills/keepachangelog/SKILL.md)
* **Source:** [keepachangelog.com](https://keepachangelog.com)
* **Remark:** The skill gives the Keep a Changelog format in new words. It
  contains no text copied from the source.

## 2. Dependencies and build tools

### LLVM and Clang
* **Function:** C/ASM target compiler, linker (`ld.lld`), and executable tools.
  The build makes a copy of Clang freestanding headers from the compiler
  resource directory into `sysroot/usr/include` at build time.
* **Source:** [llvm.org](https://llvm.org)
* **License:** Apache License 2.0 with LLVM license clauses. Refer to the
  license texts in the LLVM repository:
  [LICENSE.TXT](https://github.com/llvm/llvm-project/blob/main/LICENSE.TXT).

### GNU GRUB
* **Function:** Bootloader. `grub-mkrescue` makes the bootable ISO in build step
  4.
* **Source:** [gnu.org/software/grub](https://www.gnu.org/software/grub)
* **License:** `GPL-3.0-or-later`. Used as a build tool. GRUB code is not part
  of the kernel or the user executables.

### QEMU
* **Function:** Machine emulator. The test harness boots the ISO headless in
  QEMU and examines the serial output.
* **Source:** [qemu.org](https://www.qemu.org)
* **License:** `GPL-2.0-or-later`. Used as a test tool at development time.

### Debian bookworm-slim
* **Function:** Base image of the build container (`Containerfile`).
* **Source:** [debian.org](https://www.debian.org)
* **License:** The Debian system and its packages have their own licenses
  (mostly GPL-family and permissive licenses). Refer to
  `/usr/share/doc/*/copyright` in the image.

### Podman
* **Function:** Container runtime that operates the development environment on
  the host.
* **Source:** [podman.io](https://podman.io)
* **License:** Apache-2.0. Used as a host tool at development time.

### Mermaid.js
* **Function:** Client-side diagram renderer for `mermaid` fences in the Sphinx
  HTML output (loaded through jsDelivr CDN with plain-text `<pre>` when
  offline).
* **Source:** [mermaid.js.org](https://mermaid.js.org)
* **License:** MIT License.

### posix-issue8-os-dev manifest
* **Component(s) Used:** `tests/posix/issue8-manifest.tsv` (factual index of
  IEEE Std 1003.1-2024 identifier names, option markers, and URLs with no
  specification prose).
* **Source:** `posix-issue8-os-dev` skill (`references/issue8-manifest.tsv`).
* **License:** MIT License. Copyright (c) 2026 Deniz Eren.

### OpenSSH Portable (9.9p2)
* **Function:** Ring 3 SSH server (`sshd`, `sshd-session`) and key generator
  (`ssh-keygen`). `cmake/openssh.cmake` downloads and compiles it for Ring 3
  during the CMake build (SHA-256
  `91aadb603e08cc285eddf965e1199d02585fa94d994d6cae5b41e1721e215673`). This
  repository does not keep upstream source files.
* **Source:** [openssh.com][openssh-src]
* **License:** BSD and ISC licenses (with public-domain cryptography components
  recorded in the upstream `LICENCE` file of OpenSSH Portable).

### zlib (1.3.1)
* **Function:** Ring 3 compression library (`libz.a`, `<zlib.h>`, `<zconf.h>`).
  `cmake/zlib.cmake` downloads and compiles it for Ring 3 into the sysroot
  during the CMake build (SHA-256
  `9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23`). This
  repository does not keep upstream source files.
* **Source:** [zlib.net][zlib-src]
* **License:** zlib License (Jean-loup Gailly and Mark Adler).

### OpenSSL (3.0.15)
* **Function:** Ring 3 cryptography and TLS library (`libcrypto.a`, `libssl.a`,
  and `<openssl/*.h>`). `cmake/openssl.cmake` downloads and compiles it for Ring
  3 into the sysroot during the CMake build (SHA-256
  `23c666d0edf20f14249b3d8f0368acaee9ab585b09e1de82107c66e1f3ec9533`). This
  repository does not keep upstream source files.
* **Source:** [openssl.org][openssl-src]
* **License:** Apache License 2.0.

## 3. Trademarks

* POSIX® is a registered trademark of the Institute of Electrical and Electronic
  Engineers, Inc.
* This project operates independently, targets interfaces from POSIX.1-2024
  (IEEE Std 1003.1-2024 / ISO/IEC/IEEE 9945:2026), and makes no claim of
  official certification.

[cc-src]: https://www.conventionalcommits.org/en/v1.0.0/
[agentskills-src]:
  https://github.com/agentskills/agentskills/blob/main/docs/specification.mdx
[semver-src]: https://github.com/semver/semver.md/blob/master/semver.md
[cc-by-3]: https://creativecommons.org/licenses/by/3.0/legalcode
[openssh-src]:
  https://cdn.openbsd.org/pub/OpenBSD/OpenSSH/portable/openssh-9.9p2.tar.gz
[zlib-src]:
  https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz
[openssl-src]:
  https://www.openssl.org/source/openssl-3.0.15.tar.gz
