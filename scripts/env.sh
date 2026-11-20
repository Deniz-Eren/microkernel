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
# Usage: ./scripts/env.sh <configure|build|test|clean|shell|cmd...>
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
    clean)
        rm -rf "$ROOT/build" "$ROOT/sysroot"
        ;;
    shell)
        ensure_image
        crun bash
        ;;
    *)
        ensure_image
        crun "$@"
        ;;
esac
