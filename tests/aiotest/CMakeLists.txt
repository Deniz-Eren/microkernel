##
# @file     CMakeLists.txt
# @brief    Build configuration for the POSIX.1-2024 <aio.h> QEMU test
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(aiotest test_aio.c)
