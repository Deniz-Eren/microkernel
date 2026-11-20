##
# @file     CMakeLists.txt
# @brief    Build the Ring 3 sigtest thread, errno, and signal test executable
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(sigtest main.c)
