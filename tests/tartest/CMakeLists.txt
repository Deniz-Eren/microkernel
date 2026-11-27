##
# @file     CMakeLists.txt
# @brief    Build configuration for the Ring 3 sysroot tar verification test
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_dynamic_executable(tartest test_tar.c)
