---
name: dev-environment
description: >-
  Initialize, enter, and use the isolated development environment of
  microkernel. Use this skill before you compile, do tests, lint, or format
  code, when you add dependencies, or when environment problems occur.
---

# Development Environment

**Type:** Container (Podman, one image `microkernel-dev`, which `scripts/env.sh`
controls)

The image contains Clang, LLD, CMake, Ninja, the GRUB tools, xorriso, mtools,
QEMU, and Python 3. Podman mounts the project directory at `/workspace` in the
container.

## Initial Configuration

1. Install Podman on the host. All build tools operate only in the container.
   The host must have only Podman, git, and bash.
2. Start the configure command. On the first start, this script makes the
   container image and configures the build directory:

```bash
./scripts/env.sh configure
```

## Commands

| Command | Task |
| --- | --- |
| `./scripts/env.sh configure [args]` | Configure `build/` in the container |
| `./scripts/env.sh build [args]` | Incremental build in the container |
| `./scripts/env.sh test [args]` | Start QEMU ([cmake](../cmake/SKILL.md)) |
| `./scripts/env.sh clean` | Remove `build/` and `sysroot/` |
| `./scripts/env.sh shell` | Interactive shell in the container |
| `./scripts/enter-dev.sh` | Same as `env.sh shell` |
| `./scripts/env.sh <cmd...>` | Start a specified command in the container |

For build, test, and lint procedures, start `scripts/build-agent.sh` (refer to
the [cmake skill](../cmake/SKILL.md)).

## Dependencies

`Containerfile` specifies the apt tool packages. To add a dependency:

1. Add the package to `Containerfile`.
2. Make the image again with `podman build -t microkernel-dev .`, or remove the
   image and start an `env.sh` command.
3. Add an entry to `docs/THIRD-PARTY-NOTICES.md`.

The `MICROKERNEL_DEV_IMAGE` parameter sets the image name (default
`microkernel-dev`).

## Problem Resolution

- **`image not found` on first start:** `env.sh` makes the image automatically.
  Make sure that Podman operates without root problems (`podman info`).
- **Missing tool in container:** Add the package to `Containerfile` and make the
  image again (refer to Dependencies).
- **Expired cache after a toolchain change:** Start `./scripts/env.sh clean`,
  then start `configure` and `build`.
- **Container cannot write to `/workspace`:** Examine the SELinux volume option
  `:Z` in `scripts/env.sh`.
