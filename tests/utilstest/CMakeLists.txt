##
# @file     CMakeLists.txt
# @brief    Build configuration for Ring 3 POSIX Base headers and utilities test
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(utilstest test_utils.c)
microkernel_add_user_shared_library(dltest BOOT_NAME libdltest.so dlplugin.c)
