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
# Usage: ./scripts/env.sh <configure|build|test|run|coverage|clean|shell|cmd...>
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="${MICROKERNEL_DEV_IMAGE:-microkernel-dev}"
ARCH="${MICROKERNEL_ARCH:-x86-64}"

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
    local extra=()
    while [ "$#" -gt 0 ] && [ "$1" = "--podman-arg" ]; do
        extra+=("$2")
        shift 2
    done
    if [ -t 0 ] && [ -t 1 ]; then
        podman run --rm -it "${extra[@]}" \
            -v "$ROOT:/workspace:Z" -w /workspace "$IMAGE" "$@"
    else
        podman run --rm -i "${extra[@]}" \
            -v "$ROOT:/workspace:Z" -w /workspace "$IMAGE" "$@"
    fi
}

case "${1:-shell}" in
    configure)
        shift
        ensure_image
        crun cmake -S . -B build -G Ninja \
            -DMICROKERNEL_ARCH="$ARCH" \
            -DCMAKE_TOOLCHAIN_FILE="cmake/clang-${ARCH}.cmake" \
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
        crun python3 tests/test_runner.py --arch "$ARCH" "$@"
        ;;
    run)
        shift
        ensure_image
        NET_PORTS="${MICROKERNEL_NET_PORTS:-2222:22,8080:8080}"
        PODMAN_PORT_ARGS=()
        HOSTFWD_SPEC=""
        if [ "$NET_PORTS" != "none" ] && [ -n "$NET_PORTS" ]; then
            IFS=',' read -r -a _pairs <<< "$NET_PORTS"
            for _p in "${_pairs[@]}"; do
                _hp="${_p%%:*}"
                _gp="${_p##*:}"
                PODMAN_PORT_ARGS+=(
                    --podman-arg "-p"
                    --podman-arg "127.0.0.1:${_hp}:${_hp}"
                )
                HOSTFWD_SPEC="${HOSTFWD_SPEC},hostfwd=tcp::${_hp}-:${_gp}"
            done
        fi
        echo "[env.sh] booting interactive Microkernel shell (Ctrl-A X to exit)"
        if [ -n "$HOSTFWD_SPEC" ]; then
            echo "[env.sh] host->guest localhost ports: $NET_PORTS"
        fi
        crun_tty "${PODMAN_PORT_ARGS[@]}" bash -c '
            set -euo pipefail
            HOSTFWD_SPEC="$1"
            shift
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
            if [ ! -f build/disk.img ]; then
                dd if=/dev/zero of=build/disk.img bs=1M count=4 status=none
            fi
            exec qemu-system-x86_64 -machine q35 -cpu max \
                -m 256M -smp 2 -cdrom build/microkernel-shell.iso \
                -drive id=hd0,file=build/disk.img,if=none,format=raw \
                -device virtio-blk-pci,drive=hd0,disable-legacy=on \
                -netdev "user,id=net0${HOSTFWD_SPEC}" \
                -device virtio-net-pci,netdev=net0,disable-legacy=on \
                -serial mon:stdio -display none -no-reboot "$@"
        ' -- "$HOSTFWD_SPEC" "$@"
        ;;
    coverage)
        shift
        ensure_image
        crun bash -c '
            set -euo pipefail
            ARCH="$1"
            shift
            cmake -S . -B build-cov -G Ninja \
                -DMICROKERNEL_ARCH="$ARCH" \
                -DCMAKE_TOOLCHAIN_FILE="cmake/clang-${ARCH}.cmake" \
                -DMICROKERNEL_COVERAGE=ON "$@"
            cmake --build build-cov
            rm -rf build-cov/coverage
            mkdir -p build-cov/coverage/raw
            python3 tests/test_runner.py \
                --arch "$ARCH" \
                --iso build-cov/microkernel.iso \
                --timeout 420 \
                --expect-file scripts/qemu-expects.txt \
                --cov-out build-cov/coverage/raw \
                --wait-cov
            python3 tools/coverage/generate_report.py \
                --raw-dir build-cov/coverage/raw \
                --build-dir build-cov \
                --out-dir build-cov/coverage
        ' -- "$ARCH" "$@"
        ;;
    clean)
        rm -rf "$ROOT/build" "$ROOT/build-cov" "$ROOT/sysroot"
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
