##
# @file     CMakeLists.txt
# @brief    Build configuration for POSIX.1-2024 libc Issue 8 additions test
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(libctest test_libc_issue8.c)
