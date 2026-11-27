##
# @file     CMakeLists.txt
# @brief    Build configuration for the Ring 3 sysroot tzcode verification test
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_dynamic_executable(tztest test_tz.c)
