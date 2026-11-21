##
# @file     CMakeLists.txt
# @brief    Build configuration for forkpipetest and exechelper QEMU tests
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(forkpipetest test_forkpipe.c)
microkernel_add_user_executable(exechelper test_exechelper.c)
