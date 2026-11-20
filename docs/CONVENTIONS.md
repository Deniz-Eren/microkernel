## Purpose

Define the conventions of microkernel for file names, document formatting, and copyright headers.

## Structure

The conventions are in groups by category: file names, document formatting, document purposes, language rules, and copyright headers.

## Formatting Rules

- You must obey all conventions. A convention with the mark "recommended" is an alternative.
- Record each deviation as an ADR in [DECISIONS.md](DECISIONS.md).

---

## File Names

| Type | Rule | Examples |
|---|---|---|
| Scaffolding documents in `docs/` | UPPERCASE. One word or a hyphenated name. | `TASKS.md`, `THIRD-PARTY-NOTICES.md` |
| Other documents | A clear name | `user-guide.md` |
| Skills | `docs/skills/<lowercase-name>/SKILL.md`. The directory name is the same as the frontmatter `name` | `docs/skills/git/SKILL.md` |
| Agent definitions | `docs/agents/<lowercase-kebab>.md` | `docs/agents/build.md` |
| C sources and headers | lowercase, with no spaces | `sched.c`, `uaccess.h` |
| Test programs | one directory per program under `tests/` | `tests/sigtest/` |

## Document Formatting

- All generated markdown files in `docs/` (but not in `docs/skills/` or `docs/agents/`) start with these three headings, in this sequence:

  ```
  ## Purpose
  ## Structure
  ## Formatting Rules
  ```

- Do not put markdown links in backticks.
- Procedures are only in skill files. Other documents link to them.

## Document Purposes

| File | Contains | Must NOT contain |
|---|---|---|
| `ARCHITECTURE.md` | The current structure and components | Decisions or causes |
| `DECISIONS.md` | ADRs (dated, append-only) | The current structure |
| `TASKS.md` | Tasks with their status | General information |
| `CHANGELOG.md` (root) | The important changes, in groups by version | Descriptions of the current architecture |
| `VISION.md` | The mission and principles | Tasks |
| `CONVENTIONS.md` | Names, formatting, and copyright rules | Decisions |
| `THIRD-PARTY-NOTICES.md` | Third-party attribution | Internal architecture |

## Language Rules for Documentation

The technical documents (`README.md` and the files in `docs/`) follow ASD-STE100 Simplified Technical English, Issue 9:

- Write short sentences. One instruction per sentence.
- Use the active voice and the imperative for instructions.
- Use only approved words. Technical nouns and technical verbs come from `technical_terms.txt` at the repository root. Multi-word technical terms come from `allowed_phrases.txt`.
- When you introduce a new technical term, add it to the correct term list.
- Agents use the asd-ste100 skill to write and to check documents.

## Copyright Headers

License: **MIT License**. Copyright line: Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>

Each file with comments starts with the copyright line in the comment syntax of its language. The SPDX line comes below the copyright line.

| Files | Header |
|---|---|
| C sources and headers (`.c`, `.h`) | `// Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>` + `// SPDX-License-Identifier: MIT` |
| Assembly (`.S`) | `/* Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com> */` + `/* SPDX-License-Identifier: MIT */` |
| Python (`.py`), shell (`.sh`) | `# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>` + `# SPDX-License-Identifier: MIT` after the shebang line |
| `CMakeLists.txt`, `.cmake`, `Containerfile`, `grub.cfg` | `# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>` + `# SPDX-License-Identifier: MIT` |

**Exclusions:** markdown (`.md`, `.mdx`), `.gitignore`, license files, and files that cannot hold comments. The files in `sysroot/` are copies of the Clang freestanding headers. They keep the license of LLVM (see [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)); this project adds no headers to them.
