---
name: clang-format
description: >-
  Do a check of the C code format and repair it with clang-format in the build
  container. Use this skill after you change source files and before you make a
  commit.
---

# clang-format

The configuration file is `.clang-format` at the project root. It uses the LLVM
style with four project parameters: 4-space indent, 80 columns that agree with
`.editorconfig`, no one-line functions, and includes in their written sequence.
The tool operates in the container (`clang-format` 14).

## Commands

To do a check only (no changes):

```bash
./scripts/formatter-agent.sh
```

To repair the format directly in the source files:

```bash
./scripts/formatter-agent.sh --fix
```

To do a check of one file manually:

```bash
./scripts/env.sh clang-format --style=file --dry-run --Werror <file>
```

## Rules

- The formatter examines only `kernel/`, `libc/`, `servers/`, and `tests/`
  (`*.c`, `*.h`). It does not examine `build/`, `sysroot/`, or `docs/`.
- Do the format check before each commit that changes C code (refer to the
  [format-validator agent](../../agents/format-validator.md)).
- Do not change `.clang-format` parameters without a decision record in
  [docs/00-overview/wdr/index.md](../../00-overview/wdr/index.md).
