##
# @file     clang-x86_64.cmake
# @brief    LLVM cross-toolchain configuration for x86-64 microkernel targets
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

# LLVM cross-toolchain configuration for Microkernel. Kernel target:
# x86_64-unknown-none-elf (Ring 0). Userland uses the same triple with sysroot.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER   clang)
set(CMAKE_ASM_COMPILER clang)
set(CMAKE_AR           llvm-ar)
set(CMAKE_RANLIB       llvm-ranlib)
set(CMAKE_LINKER       ld.lld)
set(CMAKE_OBJCOPY      llvm-objcopy)
set(CMAKE_OBJDUMP      llvm-objdump)
set(CMAKE_NM           llvm-nm)
set(CMAKE_STRIP        llvm-strip)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_FLAGS_INIT   "-target x86_64-unknown-none-elf")
set(CMAKE_ASM_FLAGS_INIT "-target x86_64-unknown-none-elf")
