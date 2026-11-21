##
# @file     CMakeLists.txt
# @brief    Build configuration for the Ring 3 /dev/sock AF_UNIX socket server
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(sockserv main.c)
