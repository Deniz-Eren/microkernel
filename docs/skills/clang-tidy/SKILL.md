---
name: clang-tidy
description: >-
  Do static analysis of the C source files with clang-tidy in the build
  container. Use this skill after you change kernel, libc, server, or test code,
  and when a lint warning is not clear.
---

# clang-tidy

The configuration file is `.clang-tidy` at the project root. The active checks
are `bugprone-macro-parentheses`, `clang-analyzer-core.*`, and
`clang-analyzer-unix.*`. The tool operates in the container (`clang-tidy` 14)
and uses the compilation database from the configure step.

## Commands

To examine all source files (the build agent starts this command):

```bash
./scripts/env.sh bash -c \
  "run-clang-tidy -p build -quiet \$(find kernel libc servers tests \
  \( -name '*.c' -o -name '*.h' \))"
```

To examine one file:

```bash
./scripts/env.sh bash -c "clang-tidy -p build --quiet <file>"
```

`clang-tidy` examines one file for each start. `run-clang-tidy` examines more
than one file in parallel.

## Suppression rules

The kernel writes through the pointer arguments of the syscall wrappers in
`libc/include/sys/`. Because the static analyzer cannot examine the wrapper
bodies, it flags reads of these output parameters as uninitialized. These
warnings are incorrect.

- Suppress one warning at the call site with a named comment: `//
  NOLINT(clang-analyzer-core.CallAndMessage)` or `//
  NOLINT(clang-analyzer-core.UndefinedBinaryOperatorResult)`. Add a short
  comment that names the syscall that writes the value.
- Do not disable a full check in `.clang-tidy` because of one warning. Record a
  check change as a workflow decision in
  [docs/00-overview/wdr/index.md](../../00-overview/wdr/index.md).

## Known deviations from the defaults

- `bugprone-reserved-identifier` is off. The linker reserves `__bss_start`,
  `__text_start`, and equivalent names (refer to `WDR_0007`).
