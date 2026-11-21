##
# @file     CMakeLists.txt
# @brief    Build the Ring 3 TTY, termios, and multi-server ppoll test
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(ttytest test_tty.c)
