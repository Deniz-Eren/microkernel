##
# @file     CMakeLists.txt
# @brief    Build the Ring 3 serial TTY server (/dev/tty)
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(ttyserv main.c)
