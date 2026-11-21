##
# @file     CMakeLists.txt
# @brief    Build configuration for the stdiotest QEMU verification binary
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(stdiotest test_stdio.c)
