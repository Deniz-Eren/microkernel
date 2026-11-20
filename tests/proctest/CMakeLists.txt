##
# @file     CMakeLists.txt
# @brief    Build the Ring 3 Milestone 5 process and memory syscall test apps
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(proctest main.c)
microkernel_add_user_executable(procchild child.c)
