##
# @file     CMakeLists.txt
# @brief    Build the Ring 3 POSIX.1-2024 pthreads, semaphores, and TLS test
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(pthreadtest main.c)
