##
# @file     CMakeLists.txt
# @brief    Build configuration for the Ring 3 sysroot OpenSSL verification test
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(openssltest test_openssl.c)
target_link_libraries(openssltest PRIVATE ssl crypto)
add_dependencies(openssltest openssl_sysroot ssl crypto)
