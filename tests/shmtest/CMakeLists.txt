##
# @file     CMakeLists.txt
# @brief    Build the Ring 3 POSIX SHM, ML, and MLR verification test
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(shmtest test_shm.c)
