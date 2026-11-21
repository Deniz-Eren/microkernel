##
# @file     CMakeLists.txt
# @brief    Build the Ring 3 VFS bootfs/tmpfs/stat/dirent verification test
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(vfstest test_vfs.c)
