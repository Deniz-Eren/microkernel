---
name: dev-environment
description: >-
  Set up, enter and use the isolated development environment of microkernel. Use
  before you build, test, lint or format, when you add dependencies, or when
  environment problems occur.
---

# Development Environment

**Type:** Containerized (Podman, single image `microkernel-dev`, driven by
`scripts/env.sh`)

The image contains Clang, LLD, CMake, Ninja, the GRUB tools, xorriso, mtools,
QEMU, and Python 3. The project directory is mounted at `/workspace` in the
container.

## Setup

1. Install Podman on the host. The container is the only place where the build
   tools run; the host needs only Podman, git, and bash.
2. Run one command. On the first run it makes the container image, then it
   configures the build directory:

```bash
./scripts/env.sh configure
```

## Use

| Command | Task |
| --- | --- |
| `./scripts/env.sh configure [args]` | configure `build/` in container |
| `./scripts/env.sh build [args]` | incremental build in container |
| `./scripts/env.sh test [args]` | run QEMU ([cmake](../cmake/SKILL.md)) |
| `./scripts/env.sh clean` | remove `build/` and `sysroot/` |
| `./scripts/env.sh shell` | interactive shell in container |
| `./scripts/enter-dev.sh` | same as `env.sh shell` |
| `./scripts/env.sh <cmd...>` | run arbitrary command in container |

For the build, test, and lint procedures, use `scripts/build-agent.sh` (see the
[cmake skill](../cmake/SKILL.md)).

## Dependencies

The tool packages are in the `Containerfile` apt list. To add a dependency:

1. Add the package to the `Containerfile`.
2. Make the image again: `podman build -t microkernel-dev .` (or remove the
   image and run any `env.sh` command).
3. Add an entry to `docs/THIRD-PARTY-NOTICES.md`.

The image name comes from `MICROKERNEL_DEV_IMAGE` (default `microkernel-dev`).

## Troubleshooting

- **`image not found` on first run:** `env.sh` builds the image automatically.
  Check that Podman runs without root problems (`podman info`).
- **Missing tool in container:** Add the package to `Containerfile` and rebuild
  the image (see Dependencies).
- **Stale cache after toolchain change:** Run `./scripts/env.sh clean`, then
  `configure` and `build`.
- **Container cannot write mount:** Check the SELinux volume flag `:Z` in
  `scripts/env.sh`.
