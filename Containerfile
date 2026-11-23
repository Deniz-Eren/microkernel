# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT

# Microkernel — deterministic Podman build & headless QEMU test environment
FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        clang \
        clang-format \
        clang-tidy \
        lld \
        llvm \
        llvm-dev \
        cmake \
        ninja-build \
        pkg-config \
        grub-common \
        grub-pc-bin \
        grub-efi-amd64-bin \
        xorriso \
        mtools \
        qemu-system-x86 \
        python3 \
        python3-venv \
        python3-pip \
        e2fsprogs \
        git \
        curl \
        make \
        perl \
        openssh-client \
    && rm -rf /var/lib/apt/lists/*

# Ensure unversioned llvm and clang tool symlinks exist (Debian ships versioned names)
RUN set -eux; \
    for t in ar nm objcopy objdump ranlib readelf size strings strip clang-format clang-tidy; do \
        if ! command -v "llvm-$t" >/dev/null 2>&1; then \
            for v in 14 15 16 17 18 19 20; do \
                if [ -e "/usr/bin/llvm-$t-$v" ]; then ln -sf "/usr/bin/llvm-$t-$v" "/usr/bin/llvm-$t"; break; fi; \
            done; \
        fi; \
    done

WORKDIR /workspace
CMD ["/bin/bash"]
