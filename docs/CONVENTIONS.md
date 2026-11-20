## Purpose

Define the conventions of microkernel for file names, document formatting, and
copyright headers.

## Structure

The conventions are in groups by category: file names, document formatting,
document purposes, language rules, and copyright headers.

## Formatting Rules

- You must obey all conventions. A convention with the mark "recommended" is an
  alternative.
- Record each deviation as an ADR in [DECISIONS.md](DECISIONS.md).

---

## File Names

- **Scaffolding docs in `docs/`:** UPPERCASE, one word or hyphenated (for
  example `TASKS.md`, `THIRD-PARTY-NOTICES.md`).
- **Other documents:** A clear name (for example `user-guide.md`).
- **Skills:** `docs/skills/<lowercase-name>/SKILL.md`. Directory name matches
  frontmatter `name` (for example `docs/skills/git/SKILL.md`).
- **Agent definitions:** `docs/agents/<lowercase-kebab>.md` (for example
  `docs/agents/build.md`).
- **C sources and headers:** Lowercase with no spaces (for example `sched.c`,
  `uaccess.h`).
- **Test programs:** One directory per program under `tests/` (for example
  `tests/sigtest/`).

## Document Formatting

- All generated markdown files in `docs/` (but not in `docs/skills/` or
  `docs/agents/`) start with these three headings, in this sequence:

  ```
  ## Purpose
  ## Structure
  ## Formatting Rules
  ```

- Do not put markdown links in backticks.
- Procedures are only in skill files. Other documents link to them.

## Document Purposes

| File | Contains | Must NOT contain |
| --- | --- | --- |
| `ARCHITECTURE.md` | Current structure | Decisions or causes |
| `DECISIONS.md` | ADRs (dated, append-only) | Current structure |
| `TASKS.md` | Tasks with status | General info |
| `CHANGELOG.md` | Changes by version | Current architecture |
| `VISION.md` | Mission and principles | Tasks |
| `CONVENTIONS.md` | Names, format, copyright | Decisions |
| `THIRD-PARTY-NOTICES.md` | Third-party attribution | Internal architecture |

## Language Rules for Documentation

The technical documents (`README.md` and the files in `docs/`) follow ASD-STE100
Simplified Technical English, Issue 9:

- Write short sentences. One instruction per sentence.
- Use the active voice and the imperative for instructions.
- Use only approved words. Technical nouns and technical verbs come from
  `technical_terms.txt` at the repository root. Multi-word technical terms come
  from `allowed_phrases.txt`.
- When you introduce a new technical term, add it to the correct term list.
- Agents use the asd-ste100 skill to write and to check documents.

## Copyright Headers

License: **MIT License**. Copyright line: Copyright (c) 2026 Deniz Eren
<deniz.eren@outlook.com>

Each human-authored source file starts with a Doxygen header block matching
`[file-header]` in `.editorconfig` (see `CNV_0003` in
[records/CONVENTIONS.md](records/CONVENTIONS.md)):

- **C sources and headers (`.c`, `.h`):** `/** @file ... @brief ... @copyright`
  with `Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>` and
  `SPDX-License-Identifier: MIT`.
- **Assembly (`.S`):** `/* Copyright (c) 2026 Deniz Eren
  <deniz.eren@outlook.com> */` and `/* SPDX-License-Identifier: MIT */`.
- **Python (`.py`), shell (`.sh`), CMake (`CMakeLists.txt`, `.cmake`):** `##`
  Doxygen block with `@file`, `@brief`, and `@copyright` (after any shebang).
- **`Containerfile`, `grub.cfg`:** `# Copyright (c) 2026 Deniz Eren
  <deniz.eren@outlook.com>` and `# SPDX-License-Identifier: MIT`.

**Exclusions:** markdown (`.md`, `.mdx`), `.gitignore`, license files, and files
that cannot hold comments. The files in `sysroot/` are copies of the Clang
freestanding headers. They keep the license of LLVM (see
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)); this project adds no headers
to them.
