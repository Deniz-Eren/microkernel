## Purpose

Record the third-party open-source software, files, snippets, and libraries that microkernel uses. Give the license attribution and the compliance notices.

## Structure

One section for each third-party component. Each section gives the source, the license, and the task of the component in the project.

## Formatting Rules

- Give the license text or a link to it.
- Keep the list up to date. Add an entry when you add a third-party component.

---

This document gives the third-party open-source software, files, snippets, and libraries that this project uses. We give thanks to the open-source community for its contributions.

The notices, the terms, and the copyright lines below give the attribution for their open-source licenses. **Note:** these licenses apply only to the third-party files and dependencies in the list. They do not change the primary license of this project. The primary license is in [LICENSE.md](../LICENSE.md).

## 1. Copied files and snippets

### conventionalcommits.org
* **Component(s) Used:** [skills/git/references/COMMITS.md](skills/git/references/COMMITS.md)
* **Source:** [conventionalcommits.org (v1.0.0)](https://github.com/conventional-commits/conventionalcommits.org/blob/master/content/v1.0.0/index.md)
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
* **Component(s) Used:** [skills/create-skill/references/SKILLS.mdx](skills/create-skill/references/SKILLS.mdx)
* **Source:** [github.com/agentskills/agentskills](https://github.com/agentskills/agentskills/blob/main/docs/specification.mdx)
* **Source License:** Apache License 2.0. The full license text is in the agentskills repository at the source link above.

### Semantic Versioning 2.0.0
* **Component(s) Used:** [skills/git/references/semver.md](skills/git/references/semver.md)
* **Source:** [github.com/semver/semver.md](https://github.com/semver/semver/blob/master/semver.md)
* **Source License:** Creative Commons Attribution 3.0 Unported (CC BY 3.0). The full license text is at [creativecommons.org/licenses/by/3.0/legalcode](https://creativecommons.org/licenses/by/3.0/legalcode).

### keepachangelog.com
* **Component(s) Used:** [skills/keepachangelog/SKILL.md](skills/keepachangelog/SKILL.md)
* **Source:** [keepachangelog.com](https://keepachangelog.com)
* **Note:** The skill describes the Keep a Changelog format with its own words. No text is copied from the source.

## 2. Dependencies and build tools

### LLVM and Clang
* **Role:** the C/ASM cross-compiler, linker (`ld.lld`), and binary tools. The build copies the Clang freestanding headers from the compiler resource directory into `sysroot/usr/include` at build time.
* **Source:** [llvm.org](https://llvm.org)
* **License:** Apache License 2.0 with LLVM exceptions. See the license texts in the LLVM repository: [LICENSE.TXT](https://github.com/llvm/llvm-project/blob/main/LICENSE.TXT).

### GNU GRUB
* **Role:** boot loader. `grub-mkrescue` makes the bootable ISO in build stage 4.
* **Source:** [gnu.org/software/grub](https://www.gnu.org/software/grub)
* **License:** GPL-3.0-or-later. Used as a build tool; GRUB code is not part of the kernel or the user programs.

### QEMU
* **Role:** machine emulator. The test harness boots the built ISO headless under QEMU and checks the serial output.
* **Source:** [qemu.org](https://www.qemu.org)
* **License:** GPL-2.0-or-later. Used as a test tool at development time.

### Debian bookworm-slim
* **Role:** the base image of the build container (`Containerfile`).
* **Source:** [debian.org](https://www.debian.org)
* **License:** the Debian system and its packages carry their own licenses (mostly GPL-family and permissive licenses). See `/usr/share/doc/*/copyright` in the image.

### Podman
* **Role:** the container runtime that runs the development environment on the host.
* **Source:** [podman.io](https://podman.io)
* **License:** Apache-2.0. Used as a host tool at development time.
