##
# @file     CMakeLists.txt
# @brief    Build the Ring 3 POSIX.1-2024 monotonic clocks and timers test app
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(timetest main.c)
