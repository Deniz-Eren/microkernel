##
# @file     x86-64.cmake
# @brief    Platform build settings for the x86-64 architecture target
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

set(MICROKERNEL_TARGET_TRIPLE "x86_64-unknown-none-elf")

set(MICROKERNEL_USER_ARCH_CFLAGS
    -march=x86-64-v3
    -mno-red-zone
    -DMICROKERNEL_ARCH="x86-64"
)

set(MICROKERNEL_KERNEL_ARCH_CFLAGS
    -march=x86-64-v3
    -mcmodel=kernel
    -mno-red-zone
    -mno-mmx
    -mno-sse
    -mno-sse2
    -mno-avx
    -mno-avx2
    -mno-fma
    -mno-f16c
    -DMICROKERNEL_ARCH="x86-64"
    -DARCH_ELF_MACHINE=62
)

set(MICROKERNEL_CRT0_SRC
    ${CMAKE_SOURCE_DIR}/libc/arch/x86-64/crt0.S
)

set(MICROKERNEL_LIBC_ASM_SRCS
    ${CMAKE_SOURCE_DIR}/libc/arch/x86-64/setjmp.S
)

set(MICROKERNEL_USER_LINKER_SCRIPT
    ${CMAKE_SOURCE_DIR}/libc/arch/x86-64/user.ld
)

set(MICROKERNEL_KERNEL_LINKER_SCRIPT
    ${CMAKE_SOURCE_DIR}/kernel/arch/x86-64/linker.ld
)

set(MICROKERNEL_KERNEL_ASM_SRCS
    ${CMAKE_SOURCE_DIR}/kernel/arch/x86-64/boot.S
    ${CMAKE_SOURCE_DIR}/kernel/arch/x86-64/isr.S
    ${CMAKE_SOURCE_DIR}/kernel/arch/x86-64/switch.S
)
