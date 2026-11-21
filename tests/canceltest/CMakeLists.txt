##
# @file     CMakeLists.txt
# @brief    Build configuration for pthread cancellation and stdio lock test
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(canceltest test_cancel.c)
