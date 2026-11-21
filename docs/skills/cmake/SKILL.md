---
name: cmake
description: >-
  Configure, build, test and clean the microkernel project with CMake in the
  build container. Use when you build the system, run the tests, add a user
  program, or change the four-stage build.
---

# CMake Build

The build runs in the container (see the [dev-environment
skill](../dev-environment/SKILL.md)). The toolchain is Clang with the target
`x86_64-unknown-none-elf` (`cmake/clang-x86_64.cmake`). The C standard is C17
with warnings as errors. The build convention is out-of-source: all output goes
to `build/` and `sysroot/`.

## Commands

| Command | Task |
|---|---|
| `./scripts/env.sh configure` | configure `build/` (Ninja, toolchain) |
| `./scripts/env.sh build` | incremental build of all targets |
| `./scripts/env.sh test` | run the QEMU test harness |
| `./scripts/env.sh clean` | remove `build/` and `sysroot/` |

For each phase of work, run `scripts/build-agent.sh`. It builds, tests, and
lints, and it prints only the summary and the failures.

## The four stages

Each stage gives its output to the next stage:

1. **Headers.** The public headers from `libc/include/` and the Clang
   freestanding headers go into `sysroot/usr/include`.
2. **C library.** `libc` compiles to `sysroot/usr/lib/libc.a`. `crt0.S` becomes
   `sysroot/usr/lib/crt0.o`.
3. **User programs.** Each program links static with libc against the sysroot.
   `scripts/mk_boot_tar.py` packs the programs into `build/boot.tar` (ustar).
   The programs come from `servers/` and `tests/`.
4. **Kernel and ISO.** The kernel links at `0xFFFFFFFF80000000` with
   `kernel/linker.ld`. `grub-mkrescue` makes `build/microkernel.iso` with
   `iso/boot/grub/grub.cfg`.

## Test harness

`tests/test_runner.py` boots the ISO headless in QEMU, reads the serial output,
and asserts an ordered list of regex patterns. Exit code 0 means all patterns
matched before the timeout:

```bash
./scripts/env.sh test --timeout 120 \
    --expect '\[OK\] PMM/VMM/APIC initialized' \
    --expect 'name resolved /dev/echo' \
    --expect 'signal handler entered'
```

- Give each milestone expectation with one `--expect` option.
- A kernel panic or a missing pattern gives exit code 1.
- A milestone is complete only when its test gives exit code 0 (see
  [docs/TASKS.md](../../TASKS.md)).

## Add a user program

1. Make `tests/<name>/main.c` and `tests/<name>/CMakeLists.txt` (copy the
   pattern of `tests/sigtest/`).
2. In `CMakeLists.txt` at the root, add `add_subdirectory(tests/<name>)`. The
   `microkernel_add_user_executable` function adds the program to `boot.tar`
   automatically.
3. Add the copyright header to each new file (see
   [docs/CONVENTIONS.md](../../CONVENTIONS.md)).
4. Run the build agent.

## Static analysis

The configure step exports `build/compile_commands.json`. The linter needs it:

```bash
./scripts/env.sh bash -c "run-clang-tidy -p build -quiet <files...>"
```

See the [clang-tidy skill](../clang-tidy/SKILL.md) for the checks and the
suppression rules.

## Documentation and traceability

The build also makes the Sphinx traceability documentation in `build/docs/html`
and runs its CTest checks (unit tests, HTML build, needs export, model audit).
Configure the container build with `-DDOCS_AUTO_INSTALL=ON` once, so the docs
venv is created:

```bash
./scripts/env.sh configure -DDOCS_AUTO_INSTALL=ON
./scripts/env.sh ctest --test-dir build -L docs --output-on-failure
```

The authoring rules and the model layout are in
[docs/README.md](../../README.md).
