##
# @file     CMakeLists.txt
# @brief    Build configuration for the smptest QEMU verification binary
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(smptest test_smp.c)
