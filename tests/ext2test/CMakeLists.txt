##
# @file     CMakeLists.txt
# @brief    Build configuration for the Ring 3 /mnt ext2 persistence test
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(ext2test test_ext2.c)
