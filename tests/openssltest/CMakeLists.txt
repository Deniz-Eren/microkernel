##
# @file     CMakeLists.txt
# @brief    Build configuration for the Ring 3 sysroot OpenSSL verification test
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

# Dynamic executable: ET_DYN PIE with PT_INTERP=/boot/ld.so and DT_NEEDED
# libssl.so.3, libcrypto.so.3, and libc.so, verifying the shared sysroot OpenSSL
# end to end. The interpreter loads both libraries from boot.tar.
microkernel_add_user_dynamic_executable(openssltest test_openssl.c)
target_link_libraries(openssltest PRIVATE
    "${OPENSSL_LIBSSL_SO}" "${OPENSSL_LIBCRYPTO_SO}")
add_dependencies(openssltest opensslso)
