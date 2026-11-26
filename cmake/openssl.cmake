##
# @file     openssl.cmake
# @brief    Download and cross-compile OpenSSL into the Ring 3 sysroot
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

# need:: COMP_LIBC_0030 COMP_LIBC_0034

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
    set(OPENSSL_PIC_BLD "${OPENSSL_BLD}/pic")
    set(OPENSSL_TARGET_CONF "${CMAKE_SOURCE_DIR}/cmake/openssl_target.conf")
    set(OPENSSL_COMPAT_DIR "${CMAKE_SOURCE_DIR}/cmake/openssl_compat")
    set(OPENSSL_BUILD_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/build_openssl.sh")
    set(OPENSSL_LIBCRYPTO "${SYSROOT_DIR}/usr/lib/libcrypto.a")
    set(OPENSSL_LIBSSL "${SYSROOT_DIR}/usr/lib/libssl.a")
    set(OPENSSL_LIBCRYPTO_PIC "${OPENSSL_PIC_BLD}/libcrypto.a")
    set(OPENSSL_LIBSSL_PIC "${OPENSSL_PIC_BLD}/libssl.a")
    set(OPENSSL_LIBCRYPTO_SO "${OPENSSL_BLD}/libcrypto.so.3")
    set(OPENSSL_LIBSSL_SO "${OPENSSL_BLD}/libssl.so.3")
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

    #--------------------------------------------------------------------------
    # Shared libcrypto.so.3 and libssl.so.3 (-fPIC) beside the static archives.
    # Undefined libc references stay undefined at link time and resolve at load
    # time from libc.so through /boot/ld.so (ADR_0059); linking against libc.so
    # records DT_NEEDED libc.so, and linking against libcrypto.so.3 records
    # DT_NEEDED libcrypto.so.3 for libssl.so.3.
    #--------------------------------------------------------------------------
    add_custom_command(
        OUTPUT "${OPENSSL_LIBCRYPTO_SO}"
        COMMAND ${CMAKE_LINKER} -shared -soname libcrypto.so.3
                -z separate-code
                --hash-style=both
                --build-id=none
                --whole-archive "${OPENSSL_LIBCRYPTO_PIC}" --no-whole-archive
                ${LIBC_SO_OUT}
                -o "${OPENSSL_LIBCRYPTO_SO}"
        DEPENDS "${OPENSSL_HDR_STAMP}" libcso
        COMMENT "Stage 2: linking shared libcrypto.so.3"
        VERBATIM
    )
    add_custom_command(
        OUTPUT "${OPENSSL_LIBSSL_SO}"
        COMMAND ${CMAKE_LINKER} -shared -soname libssl.so.3
                -z separate-code
                --hash-style=both
                --build-id=none
                --whole-archive "${OPENSSL_LIBSSL_PIC}" --no-whole-archive
                "${OPENSSL_LIBCRYPTO_SO}" ${LIBC_SO_OUT}
                -o "${OPENSSL_LIBSSL_SO}"
        DEPENDS "${OPENSSL_HDR_STAMP}" "${OPENSSL_LIBCRYPTO_SO}"
        COMMENT "Stage 2: linking shared libssl.so.3"
        VERBATIM
    )
    add_custom_target(opensslso ALL
        DEPENDS "${OPENSSL_LIBCRYPTO_SO}" "${OPENSSL_LIBSSL_SO}"
    )
    add_custom_command(
        OUTPUT "${SYSROOT_DIR}/usr/lib/libcrypto.so.3"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${OPENSSL_LIBCRYPTO_SO}"
                "${SYSROOT_DIR}/usr/lib/libcrypto.so.3"
        DEPENDS "${OPENSSL_LIBCRYPTO_SO}"
        COMMENT "Stage 2: installing libcrypto.so.3 into the sysroot"
        VERBATIM
    )
    add_custom_command(
        OUTPUT "${SYSROOT_DIR}/usr/lib/libcrypto.so"
        COMMAND ${CMAKE_COMMAND} -E create_symlink libcrypto.so.3
                "${SYSROOT_DIR}/usr/lib/libcrypto.so"
        DEPENDS "${SYSROOT_DIR}/usr/lib/libcrypto.so.3"
        COMMENT "Stage 2: creating the libcrypto.so symlink"
        VERBATIM
    )
    add_custom_command(
        OUTPUT "${SYSROOT_DIR}/usr/lib/libssl.so.3"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${OPENSSL_LIBSSL_SO}" "${SYSROOT_DIR}/usr/lib/libssl.so.3"
        DEPENDS "${OPENSSL_LIBSSL_SO}"
        COMMENT "Stage 2: installing libssl.so.3 into the sysroot"
        VERBATIM
    )
    add_custom_command(
        OUTPUT "${SYSROOT_DIR}/usr/lib/libssl.so"
        COMMAND ${CMAKE_COMMAND} -E create_symlink libssl.so.3
                "${SYSROOT_DIR}/usr/lib/libssl.so"
        DEPENDS "${SYSROOT_DIR}/usr/lib/libssl.so.3"
        COMMENT "Stage 2: creating the libssl.so symlink"
        VERBATIM
    )
    add_custom_target(opensslso_install DEPENDS
        "${SYSROOT_DIR}/usr/lib/libcrypto.so.3"
        "${SYSROOT_DIR}/usr/lib/libcrypto.so"
        "${SYSROOT_DIR}/usr/lib/libssl.so.3"
        "${SYSROOT_DIR}/usr/lib/libssl.so")
    add_dependencies(opensslso opensslso_install)
    set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_MODULES
        "libcrypto.so.3=${OPENSSL_LIBCRYPTO_SO}")
    set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_MODULES
        "libssl.so.3=${OPENSSL_LIBSSL_SO}")
    set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_DEPS opensslso)

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
