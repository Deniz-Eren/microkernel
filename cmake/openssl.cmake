##
# @file     openssl.cmake
# @brief    Download and cross-compile OpenSSL into the Ring 3 sysroot
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

# need:: COMP_LIBC_0030

option(MICROKERNEL_BUILD_OPENSSL
    "Download and cross-compile OpenSSL into the Ring 3 sysroot" ON)

if(MICROKERNEL_BUILD_OPENSSL)
    include(FetchContent)

    set(MICROKERNEL_OPENSSL_VERSION "3.0.15")
    string(CONCAT MICROKERNEL_OPENSSL_DEFAULT_URL
        "https://github.com/openssl/openssl/releases/download/"
        "openssl-${MICROKERNEL_OPENSSL_VERSION}/"
        "openssl-${MICROKERNEL_OPENSSL_VERSION}.tar.gz")
    set(MICROKERNEL_OPENSSL_URL
        "${MICROKERNEL_OPENSSL_DEFAULT_URL}"
        CACHE STRING "OpenSSL release archive URL")
    string(CONCAT MICROKERNEL_OPENSSL_DEFAULT_SHA256
        "23c666d0edf20f14249b3d8f0368acae"
        "e9ab585b09e1de82107c66e1f3ec9533")
    set(MICROKERNEL_OPENSSL_SHA256
        "${MICROKERNEL_OPENSSL_DEFAULT_SHA256}"
        CACHE STRING "OpenSSL release archive SHA-256")

    FetchContent_Declare(
        openssl
        URL "${MICROKERNEL_OPENSSL_URL}"
        URL_HASH "SHA256=${MICROKERNEL_OPENSSL_SHA256}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_GetProperties(openssl)
    if(NOT openssl_POPULATED)
        FetchContent_Populate(openssl)
    endif()

    set(OPENSSL_SRC "${openssl_SOURCE_DIR}")
    set(OPENSSL_BLD "${CMAKE_BINARY_DIR}/openssl-build")
    set(OPENSSL_TARGET_CONF "${CMAKE_SOURCE_DIR}/cmake/openssl_target.conf")
    set(OPENSSL_COMPAT_DIR "${CMAKE_SOURCE_DIR}/cmake/openssl_compat")
    set(OPENSSL_BUILD_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/build_openssl.sh")
    set(OPENSSL_LIBCRYPTO "${SYSROOT_DIR}/usr/lib/libcrypto.a")
    set(OPENSSL_LIBSSL "${SYSROOT_DIR}/usr/lib/libssl.a")
    set(OPENSSL_HDR_STAMP "${OPENSSL_BLD}/install.stamp")

    add_custom_command(
        OUTPUT "${OPENSSL_LIBCRYPTO}" "${OPENSSL_LIBSSL}" "${OPENSSL_HDR_STAMP}"
        COMMAND bash "${OPENSSL_BUILD_SCRIPT}"
                "${OPENSSL_SRC}"
                "${OPENSSL_BLD}"
                "${OPENSSL_TARGET_CONF}"
                "${OPENSSL_COMPAT_DIR}"
                "${SYSROOT_DIR}"
                "${CLANG_RESOURCE_DIR}"
                "${OPENSSL_HDR_STAMP}"
        DEPENDS install_headers
                "${OPENSSL_TARGET_CONF}"
                "${OPENSSL_BUILD_SCRIPT}"
                "${OPENSSL_COMPAT_DIR}/memory.h"
                "${OPENSSL_COMPAT_DIR}/microkernel_compat.c"
        COMMENT "Stage 2: building OpenSSL ${MICROKERNEL_OPENSSL_VERSION}"
        VERBATIM
    )

    add_custom_target(openssl_sysroot ALL
        DEPENDS "${OPENSSL_LIBCRYPTO}" "${OPENSSL_LIBSSL}"
                "${OPENSSL_HDR_STAMP}"
    )

    add_library(crypto STATIC IMPORTED GLOBAL)
    set_target_properties(crypto PROPERTIES
        IMPORTED_LOCATION "${OPENSSL_LIBCRYPTO}"
    )
    add_dependencies(crypto openssl_sysroot c)
    target_link_libraries(crypto INTERFACE c)

    add_library(ssl STATIC IMPORTED GLOBAL)
    set_target_properties(ssl PROPERTIES
        IMPORTED_LOCATION "${OPENSSL_LIBSSL}"
    )
    add_dependencies(ssl openssl_sysroot crypto c)
    target_link_libraries(ssl INTERFACE crypto c)
endif()
