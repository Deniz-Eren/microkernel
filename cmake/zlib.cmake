##
# @file     zlib.cmake
# @brief    Download and cross-compile zlib into the Ring 3 sysroot
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

# need:: COMP_LIBC_0029

option(MICROKERNEL_BUILD_ZLIB
    "Download and cross-compile zlib into the Ring 3 sysroot" ON)

if(MICROKERNEL_BUILD_ZLIB)
    include(FetchContent)

    set(MICROKERNEL_ZLIB_VERSION "1.3.1")
    string(CONCAT MICROKERNEL_ZLIB_DEFAULT_URL
        "https://github.com/madler/zlib/releases/download/"
        "v${MICROKERNEL_ZLIB_VERSION}/zlib-${MICROKERNEL_ZLIB_VERSION}.tar.gz")
    set(MICROKERNEL_ZLIB_URL
        "${MICROKERNEL_ZLIB_DEFAULT_URL}"
        CACHE STRING "zlib release archive URL")
    string(CONCAT MICROKERNEL_ZLIB_DEFAULT_SHA256
        "9a93b2b7dfdac77ceba5a558a580e746"
        "67dd6fede4585b91eefb60f03b72df23")
    set(MICROKERNEL_ZLIB_SHA256
        "${MICROKERNEL_ZLIB_DEFAULT_SHA256}"
        CACHE STRING "zlib release archive SHA-256")

    FetchContent_Declare(
        zlib
        URL "${MICROKERNEL_ZLIB_URL}"
        URL_HASH "SHA256=${MICROKERNEL_ZLIB_SHA256}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_GetProperties(zlib)
    if(NOT zlib_POPULATED)
        FetchContent_Populate(zlib)
    endif()

    set(ZLIB_SRC "${zlib_SOURCE_DIR}")
    set(ZLIB_GEN "${CMAKE_BINARY_DIR}/zlib")
    file(MAKE_DIRECTORY "${ZLIB_GEN}")

    set(Z_PREFIX OFF)
    set(Z_HAVE_UNISTD_H ON)
    configure_file(
        "${ZLIB_SRC}/zconf.h.cmakein"
        "${ZLIB_GEN}/zconf.h"
        @ONLY
    )

    add_custom_target(install_zlib_headers
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SYSROOT_DIR}/usr/include
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ZLIB_SRC}/zlib.h" "${SYSROOT_DIR}/usr/include/zlib.h"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ZLIB_GEN}/zconf.h" "${SYSROOT_DIR}/usr/include/zconf.h"
        DEPENDS install_headers "${ZLIB_SRC}/zlib.h" "${ZLIB_GEN}/zconf.h"
        COMMENT "Stage 2: installing zlib headers into sysroot"
    )

    set(ZLIB_CFLAGS
        -ffreestanding
        -nostdlib
        -nostdinc
        -fno-builtin
        -fno-stack-protector
        -fno-pic
        -fno-pie
        -fno-asynchronous-unwind-tables
        ${MICROKERNEL_USER_ARCH_CFLAGS}
        -std=gnu17
        -w
        -D_GNU_SOURCE
        -D_DEFAULT_SOURCE
        -DHAVE_UNISTD_H=1
        -DHAVE_STDARG_H=1
        -DZ_HAVE_UNISTD_H=1
        -D_LARGEFILE64_SOURCE=0
    )

    set(ZLIB_SRCS
        adler32.c
        compress.c
        crc32.c
        deflate.c
        gzclose.c
        gzlib.c
        gzread.c
        gzwrite.c
        infback.c
        inffast.c
        inflate.c
        inftrees.c
        trees.c
        uncompr.c
        zutil.c
    )
    list(TRANSFORM ZLIB_SRCS PREPEND "${ZLIB_SRC}/")

    add_library(z STATIC ${ZLIB_SRCS})
    target_compile_options(z PRIVATE ${ZLIB_CFLAGS})
    target_include_directories(z SYSTEM PRIVATE
        "${ZLIB_GEN}"
        "${ZLIB_SRC}"
        "${SYSROOT_DIR}/usr/include"
        "${CLANG_RESOURCE_DIR}/include"
    )
    set_target_properties(z PROPERTIES
        ARCHIVE_OUTPUT_NAME "z"
        ARCHIVE_OUTPUT_DIRECTORY ${SYSROOT_DIR}/usr/lib
        SUFFIX ".a"
    )
    target_link_libraries(z INTERFACE c)
    add_dependencies(z install_zlib_headers c)
endif()
