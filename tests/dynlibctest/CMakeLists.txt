##
# @file     CMakeLists.txt
# @brief    Build configuration for the shared libc.so Ring 3 test
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

# Static driver: verifies the sysroot bootfs images exist and execs the dynamic
# executable linked against libc.so.
microkernel_add_user_executable(dynlibctest test_dynlibctest.c)

# Shared library under test for the runtime dlopen path of /boot/ld.so. The
# bootfs name must stay distinct from utilstest's libdltest.so plugin.
microkernel_add_user_shared_library(libdldyn BOOT_NAME libdldyn.so
                                    test_dltest.c)

# Dynamic executable under test: ET_DYN PIE with PT_INTERP=/boot/ld.so and
# DT_NEEDED=libc.so, exercising stdio, malloc, pthreads, TLS errno, signals,
# fork/execve, and dlopen through the shared C library.
microkernel_add_user_dynamic_executable(dynlibctest_dyn test_dynlibctest_dyn.c)
