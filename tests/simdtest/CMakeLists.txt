##
# @file     CMakeLists.txt
# @brief    Build configuration for the x86-64-v3 SIMD and XSAVE test binary
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(simdtest test_simd.c)
