##
# @file     CMakeLists.txt
# @brief    Build configuration for the Ring 3 sysroot zlib verification test
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

# Dynamic executable: ET_DYN PIE with PT_INTERP=/boot/ld.so and DT_NEEDED
# libz.so.1 plus libc.so, verifying the shared sysroot zlib end to end.
microkernel_add_user_dynamic_executable(zlibtest test_zlib.c)
target_link_libraries(zlibtest PRIVATE "${ZLIB_SO_OUT}")
add_dependencies(zlibtest zlibso)
