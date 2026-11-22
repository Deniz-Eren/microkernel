##
# @file     CMakeLists.txt
# @brief    Build configuration for Ring 3 TTY job control and utilities test
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(jobtest test_job.c)
