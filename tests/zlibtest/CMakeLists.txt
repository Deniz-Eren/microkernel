##
# @file     CMakeLists.txt
# @brief    Build configuration for the Ring 3 sysroot zlib verification test
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(zlibtest test_zlib.c)
target_link_libraries(zlibtest PRIVATE z)
add_dependencies(zlibtest z)
