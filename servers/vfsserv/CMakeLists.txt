##
# @file     CMakeLists.txt
# @brief    Build the Ring 3 VFS server (bootfs and tmpfs over /dev/vfs)
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(vfsserv main.c)
