##
# @file     CMakeLists.txt
# @brief    Build configuration for Ring 3 POSIX XCU sh and core utilities
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(sh sh/main.c)
microkernel_add_user_executable(echo utils/echo.c)
microkernel_add_user_executable(cat utils/cat.c)
microkernel_add_user_executable(ls utils/ls.c)
microkernel_add_user_executable(true utils/true.c)
microkernel_add_user_executable(false utils/false.c)
microkernel_add_user_executable(util_test BOOT_NAME test utils/test.c)
