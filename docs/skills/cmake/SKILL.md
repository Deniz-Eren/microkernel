---
name: cmake
description: >-
  Configure, compile, do tests of, and clean the microkernel project with CMake
  in the build container. Use this skill when you compile the system, do the
  tests, add a user executable, or change the four-step build.
---

# CMake Build

The build operates in the container (refer to the [dev-environment
skill](../dev-environment/SKILL.md)). The toolchain is Clang with the target
`x86_64-unknown-none-elf` (`cmake/clang-x86-64.cmake`,
`cmake/arch/x86-64.cmake`, `-DMICROKERNEL_ARCH=x86-64`). The C standard is C17
with warnings as errors. All build output goes to `build/` and `sysroot/`.

## Commands

| Command | Task |
| --- | --- |
| `./scripts/env.sh configure` | Configure `build/` (Ninja, toolchain) |
| `./scripts/env.sh build` | Incremental build of all targets |
| `./scripts/env.sh test` | Start the QEMU test harness |
| `./scripts/env.sh coverage` | Make `build-cov/` and write HTML/LCOV coverage |
| `./scripts/env.sh clean` | Remove `build/`, `build-cov/`, and `sysroot/` |

For each step of work, start `scripts/build-agent.sh`. This script compiles,
does tests, and does static analysis, and it writes only the summary and errors.

## The four steps

Each step gives its output to the next step:

1. **Headers.** CMake puts copies of the public headers from `libc/include/` and
   the Clang freestanding headers into `sysroot/usr/include`.
2. **C library.** Clang compiles `libc` into `sysroot/usr/lib/libc.a` and
   compiles `crt0.S` into `sysroot/usr/lib/crt0.o`.
3. **User executables.** Each executable links statically with `libc` and
   `sysroot`. `scripts/mk_boot_tar.py` puts the executables from `servers/` and
   `tests/` into `build/boot.tar` (ustar).
4. **Kernel and ISO.** The linker links the kernel at `0xFFFFFFFF80000000` with
   `kernel/arch/x86-64/linker.ld`. `grub-mkrescue` makes `build/microkernel.iso`
   with `iso/boot/grub/grub.cfg`.

## Test harness

`tests/test_runner.py` boots the ISO in QEMU with no display, reads serial
output, and does a check of regex patterns. Exit code 0 shows that all patterns
matched before the timeout:

```bash
./scripts/env.sh test --timeout 120 \
    --expect '\[OK\] PMM/VMM/APIC initialized' \
    --expect 'name resolved /dev/echo' \
    --expect 'signal handler entered'
```

- Give each milestone pattern with one `--expect` option.
- A kernel panic or a missing pattern gives exit code 1.
- A milestone is completed only when its test gives exit code 0 (refer to
  [docs/records/CONVENTIONS.md](../../records/CONVENTIONS.md), `CNV_0006`).
- For harness information and error diagnosis, refer to
  [docs/60-verification/qemu-harness.md](../../60-verification/qemu-harness.md)
  and the [qemu-debug skill](../qemu-debug/SKILL.md).

## Add a user executable

1. Make `tests/<name>/main.c` and `tests/<name>/CMakeLists.txt` based on
   `tests/sigtest/`.
2. In `CMakeLists.txt` at the root, add `add_subdirectory(tests/<name>)`. The
   `microkernel_add_user_executable` function adds the executable to `boot.tar`
   automatically.
3. Add the copyright header to each new file (refer to
   [docs/records/CONVENTIONS.md](../../records/CONVENTIONS.md), `CNV_0003`).
4. Start the build agent.

## Static analysis

The configure step exports `build/compile_commands.json`, which the linter uses:

```bash
./scripts/env.sh bash -c "run-clang-tidy -p build -quiet <files...>"
```

Refer to the [clang-tidy skill](../clang-tidy/SKILL.md) for the checks and
suppression rules.

## Documents and traceability

The build also makes the Sphinx traceability documents in `build/docs/html` and
starts its CTest checks (unit tests, HTML build, `needs.json` output, and model
check). Configure the container build with `-DDOCS_AUTO_INSTALL=ON` one time to
make the documents virtual environment:

```bash
./scripts/env.sh configure -DDOCS_AUTO_INSTALL=ON
./scripts/env.sh ctest --test-dir build -L docs --output-on-failure
```

The rules and model layout are in [docs/README.md](../../README.md).
