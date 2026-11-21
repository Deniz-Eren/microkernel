#!/usr/bin/env bash
##
# @file     env.sh
# @brief    Podman development container wrapper for build, test, and shell runs
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

# Microkernel Podman environment wrapper.
#
# Usage: ./scripts/env.sh <configure|build|test|run|clean|shell|cmd...>
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="${MICROKERNEL_DEV_IMAGE:-microkernel-dev}"

ensure_image() {
    if ! podman image exists "$IMAGE"; then
        echo "[env.sh] building container image $IMAGE ..."
        podman build -t "$IMAGE" "$ROOT"
    fi
}

crun() {
    podman run --rm -v "$ROOT:/workspace:Z" -w /workspace "$IMAGE" "$@"
}

crun_tty() {
    if [ -t 0 ] && [ -t 1 ]; then
        podman run --rm -it -v "$ROOT:/workspace:Z" -w /workspace "$IMAGE" "$@"
    else
        podman run --rm -i -v "$ROOT:/workspace:Z" -w /workspace "$IMAGE" "$@"
    fi
}

case "${1:-shell}" in
    configure)
        shift
        ensure_image
        crun cmake -S . -B build -G Ninja \
            -DCMAKE_TOOLCHAIN_FILE=cmake/clang-x86_64.cmake \
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "$@"
        ;;
    build)
        shift
        ensure_image
        crun cmake --build build "$@"
        ;;
    test)
        shift
        ensure_image
        crun python3 tests/test_runner.py "$@"
        ;;
    run)
        shift
        ensure_image
        echo "[env.sh] booting interactive Microkernel shell (Ctrl-A X to exit)"
        crun_tty bash -c '
            set -euo pipefail
            if [ ! -f build/kernel.elf ] || [ ! -f build/boot.tar ]; then
                cmake --build build
            fi
            mkdir -p build/iso-sh/boot/grub
            cat <<EOF > build/iso-sh/boot/grub/grub.cfg
set timeout=0
set default=0
menuentry "Microkernel (Interactive Shell)" {
    multiboot2 /boot/kernel.elf init=sh
    module2 /boot/boot.tar bootfs
    boot
}
EOF
            cp build/kernel.elf build/iso-sh/boot/kernel.elf
            cp build/boot.tar build/iso-sh/boot/boot.tar
            grub-mkrescue -o build/microkernel-shell.iso build/iso-sh \
                >/dev/null 2>&1
            exec qemu-system-x86_64 -machine q35 -cpu qemu64,+smap,+smep \
                -m 256M -smp 2 -cdrom build/microkernel-shell.iso \
                -serial mon:stdio -display none -no-reboot "$@"
        ' -- "$@"
        ;;
    clean)
        rm -rf "$ROOT/build" "$ROOT/sysroot"
        ;;
    shell)
        ensure_image
        crun_tty bash
        ;;
    *)
        ensure_image
        crun "$@"
        ;;
esac
