---
name: clang-format
description: >-
  Check and fix the C code formatting of the project with clang-format in the
  build container. Use after you edit source files, and before you commit.
---

# clang-format

The configuration is `.clang-format` at the project root: the LLVM preset with
four project parameters (4-space indent, 80 columns matching `.editorconfig`, no
single-line functions, includes in their written order). The tool runs in the
container (clang-format 14).

## Commands

Check only (no changes):

```bash
./scripts/formatter-agent.sh
```

Fix in place:

```bash
./scripts/formatter-agent.sh --fix
```

One file by hand:

```bash
./scripts/env.sh clang-format --style=file --dry-run --Werror <file>
```

## Rules

- The formatter examines only `kernel/`, `libc/`, `servers/`, and `tests/`
  (`*.c`, `*.h`). It does not examine `build/`, `sysroot/`, or `docs/`.
- Run the check before each commit that touches C code (see the
  [format-validator agent](../../agents/format-validator.md)).
- Do not change `.clang-format` parameters without an ADR in
  [docs/DECISIONS.md](../../DECISIONS.md).
