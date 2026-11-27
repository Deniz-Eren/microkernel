##
# @file     CMakeLists.txt
# @brief    Build configuration for the /boot/ld.so ldd and ldconfig test
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

# Dynamic driver: forks children that exec /boot/ldd in ldd trace mode against
# zlibtest, openssltest, and sshd, then execs zlibtest with
# LD_TRACE_LOADED_OBJECTS=1 and /boot/ldconfig.
microkernel_add_user_dynamic_executable(lddtest test_ldd.c)
