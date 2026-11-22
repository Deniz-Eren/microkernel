##
# @file     CMakeLists.txt
# @brief    Build configuration for Ring 3 command shell (sh) and utilities
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
microkernel_add_user_executable(mkdir utils/mkdir.c)
microkernel_add_user_executable(rm utils/rm.c)
microkernel_add_user_executable(cp utils/cp.c)
microkernel_add_user_executable(mv utils/mv.c)
microkernel_add_user_executable(uname utils/uname.c)
microkernel_add_user_executable(env utils/env.c)
microkernel_add_user_executable(wc utils/wc.c)
microkernel_add_user_executable(head utils/head.c)
