---
name: clang-tidy
description: >-
  Run static analysis on the C sources with clang-tidy in the build container.
  Use after you edit kernel, libc, server, or test code, and when a lint finding
  is not clear.
---

# clang-tidy

The configuration is `.clang-tidy` at the project root. The enabled checks are
`bugprone-macro-parentheses`, `clang-analyzer-core.*`, and
`clang-analyzer-unix.*`. The tool runs in the container (clang-tidy 14) and
needs the compilation database from the configure step.

## Commands

All sources (the build agent runs this):

```bash
./scripts/env.sh bash -c \
  "run-clang-tidy -p build -quiet \$(find kernel libc servers tests \
  \( -name '*.c' -o -name '*.h' \))"
```

One file:

```bash
./scripts/env.sh bash -c "clang-tidy -p build --quiet <file>"
```

`clang-tidy` takes one file per run; `run-clang-tidy` takes many and runs them
in parallel.

## Suppression rules

The kernel writes through the pointer arguments of the syscall wrappers in
`libc/include/sys/`. The static analyzer cannot see the wrapper bodies, so it
flags the reads of such out-params as uninitialized. These findings are false
positives.

- Suppress one finding at the call site with a named comment: `//
  NOLINT(clang-analyzer-core.CallAndMessage)` or `//
  NOLINT(clang-analyzer-core.UndefinedBinaryOperatorResult)`, with a short
  comment that names the filling system call.
- Do not disable a whole check in `.clang-tidy` because of one finding. Record a
  check change as an ADR in [docs/DECISIONS.md](../../DECISIONS.md).

## Known deviations from the defaults

- `bugprone-reserved-identifier` is off. The linker symbols `__bss_start`,
  `__text_start`, and similar names are intentionally reserved (see ADR-6).
