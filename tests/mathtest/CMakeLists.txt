##
# @file     CMakeLists.txt
# @brief    Build configuration for the POSIX.1-2024 math and fenv test binary
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(mathtest test_math.c)
