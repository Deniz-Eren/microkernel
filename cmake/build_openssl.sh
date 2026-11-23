#!/usr/bin/env bash
##
# @file     build_openssl.sh
# @brief    Configure and cross-compile OpenSSL 3.0 static libs into sysroot
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

# need:: COMP_LIBC_0030

set -euo pipefail

OPENSSL_SRC="$1"
OPENSSL_BLD="$2"
OPENSSL_CONF="$3"
OPENSSL_COMPAT="$4"
SYSROOT_DIR="$5"
CLANG_RES_DIR="$6"
STAMP_FILE="$7"

mkdir -p "$OPENSSL_BLD" "$SYSROOT_DIR/usr/lib" \
    "$SYSROOT_DIR/usr/include/openssl"
cd "$OPENSSL_BLD"

if [ ! -f Makefile ] || [ "$OPENSSL_CONF" -nt Makefile ] || \
   [ "$0" -nt Makefile ]; then
    perl "$OPENSSL_SRC/Configure" microkernel-x86_64 \
        --config="$OPENSSL_CONF" \
        --prefix=/usr \
        --openssldir=/mnt/etc/ssl \
        --with-rand-seed=devrandom \
        no-shared no-asm no-dso no-engine no-dynamic-engine \
        no-tests no-ui-console no-afalgeng \
        no-async no-legacy no-sctp no-srp no-nextprotoneg \
        no-cmp no-cms no-ts no-ocsp no-ct no-gost no-aria \
        no-bf no-blake2 no-camellia no-cast no-idea no-md2 \
        no-md4 no-mdc2 no-rc2 no-rc4 no-rc5 no-rmd160 no-seed \
        no-sm2 no-sm3 no-sm4 no-whirlpool \
        -isystem "$OPENSSL_COMPAT" \
        -isystem "$SYSROOT_DIR/usr/include" \
        -isystem "$CLANG_RES_DIR/include"
fi

make -j"$(nproc)" build_generated
make -j"$(nproc)" libcrypto.a libssl.a
clang -O2 -ffreestanding -nostdlib -nostdinc -fno-builtin \
    -fno-stack-protector -fno-pic -fno-pie -fno-asynchronous-unwind-tables \
    -target x86_64-unknown-none-elf -march=x86-64-v3 -mno-red-zone \
    -std=gnu17 -w \
    -isystem "$OPENSSL_COMPAT" \
    -isystem "$SYSROOT_DIR/usr/include" \
    -isystem "$CLANG_RES_DIR/include" \
    -c "$OPENSSL_COMPAT/microkernel_compat.c" \
    -o "$OPENSSL_BLD/microkernel_compat.o"
llvm-ar rcs libcrypto.a "$OPENSSL_BLD/microkernel_compat.o"
cp -f libcrypto.a "$SYSROOT_DIR/usr/lib/libcrypto.a"
cp -f libssl.a "$SYSROOT_DIR/usr/lib/libssl.a"
cp -f "$OPENSSL_SRC/include/openssl/"*.h "$SYSROOT_DIR/usr/include/openssl/"
cp -f "$OPENSSL_BLD/include/openssl/"*.h "$SYSROOT_DIR/usr/include/openssl/"
touch "$STAMP_FILE"
