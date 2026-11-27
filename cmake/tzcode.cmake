##
# @file     tzcode.cmake
# @brief    Download and cross-compile IANA tzcode and tzdata for Ring 3
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

# need:: COMP_LIBC_0038

option(MICROKERNEL_BUILD_TZCODE
    "Download and cross-compile IANA tzcode into the Ring 3 sysroot" ON)

if(MICROKERNEL_BUILD_TZCODE)
    include(FetchContent)

    set(MICROKERNEL_TZCODE_VERSION "2025b")
    string(CONCAT MICROKERNEL_TZCODE_DEFAULT_URL
        "https://data.iana.org/time-zones/releases/"
        "tzcode${MICROKERNEL_TZCODE_VERSION}.tar.gz")
    set(MICROKERNEL_TZCODE_URL
        "${MICROKERNEL_TZCODE_DEFAULT_URL}"
        CACHE STRING "IANA tzcode release archive URL")
    string(CONCAT MICROKERNEL_TZCODE_DEFAULT_SHA256
        "05f8fedb3525ee70d49c87d3fae78a8a"
        "0dbae4fe87aa565c65cda9948ae135ec")
    set(MICROKERNEL_TZCODE_SHA256
        "${MICROKERNEL_TZCODE_DEFAULT_SHA256}"
        CACHE STRING "IANA tzcode release archive SHA-256")

    string(CONCAT MICROKERNEL_TZDATA_DEFAULT_URL
        "https://data.iana.org/time-zones/releases/"
        "tzdata${MICROKERNEL_TZCODE_VERSION}.tar.gz")
    set(MICROKERNEL_TZDATA_URL
        "${MICROKERNEL_TZDATA_DEFAULT_URL}"
        CACHE STRING "IANA tzdata release archive URL")
    string(CONCAT MICROKERNEL_TZDATA_DEFAULT_SHA256
        "11810413345fc7805017e27ea9fa4885"
        "fd74cd61b2911711ad038f5d28d71474")
    set(MICROKERNEL_TZDATA_SHA256
        "${MICROKERNEL_TZDATA_DEFAULT_SHA256}"
        CACHE STRING "IANA tzdata release archive SHA-256")

    FetchContent_Declare(
        tzcode
        URL "${MICROKERNEL_TZCODE_URL}"
        URL_HASH "SHA256=${MICROKERNEL_TZCODE_SHA256}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_GetProperties(tzcode)
    if(NOT tzcode_POPULATED)
        FetchContent_Populate(tzcode)
    endif()

    FetchContent_Declare(
        tzdata
        URL "${MICROKERNEL_TZDATA_URL}"
        URL_HASH "SHA256=${MICROKERNEL_TZDATA_SHA256}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_GetProperties(tzdata)
    if(NOT tzdata_POPULATED)
        FetchContent_Populate(tzdata)
    endif()

    set(TZCODE_SRC "${tzcode_SOURCE_DIR}")
    set(TZDATA_SRC "${tzdata_SOURCE_DIR}")
    set(TZCODE_GEN "${CMAKE_BINARY_DIR}/tzcode")
    file(MAKE_DIRECTORY "${TZCODE_GEN}")
    file(STRINGS "${TZCODE_SRC}/version" TZCODE_VERSION)

    file(WRITE "${TZCODE_GEN}/version.h"
        "static char const PKGVERSION[]=\"(tzcode) \";\n"
        "static char const TZVERSION[]=\"${TZCODE_VERSION}\";\n"
        "static char const REPORT_BUGS_TO[]=\"tz@iana.org\";\n")
    file(WRITE "${TZCODE_GEN}/tzdir.h"
        "#ifndef TZDEFAULT\n"
        "# define TZDEFAULT \"UTC\"\n"
        "#endif\n"
        "#ifndef TZDIR\n"
        "# define TZDIR \"/usr/share/zoneinfo\"\n"
        "#endif\n")

    # ALL_STATE makes localtime.c heap-allocate union local_storage (~70 KiB) so
    # zdump runs cleanly inside the standard 64 KiB Ring 3 user stack.
    set(TZCODE_CFLAGS
        -ffreestanding
        -nostdlib
        -nostdinc
        -fno-builtin
        -fno-stack-protector
        -fPIC
        -fno-asynchronous-unwind-tables
        ${MICROKERNEL_USER_ARCH_CFLAGS}
        -std=gnu17
        -w
        -D_GNU_SOURCE
        -D_DEFAULT_SOURCE
        -DALL_STATE
    )

    set(TZCODE_INC_DIRS
        "${TZCODE_SRC}"
        "${TZCODE_GEN}"
        "${SYSROOT_DIR}/usr/include"
        "${CLANG_RESOURCE_DIR}/include"
    )

    function(microkernel_add_tzcode_executable name boot_name sysroot_subdir)
        add_executable(${name} ${ARGN})
        target_compile_options(${name} PRIVATE ${TZCODE_CFLAGS})
        target_include_directories(${name} SYSTEM PRIVATE ${TZCODE_INC_DIRS})
        target_link_options(${name} PRIVATE
            -pie
            --dynamic-linker=/boot/ld.so
            -T ${MICROKERNEL_USER_DYN_LINKER_SCRIPT}
            --build-id=none
            ${SYSROOT_DIR}/usr/lib/crt0.o
        )
        set_target_properties(${name} PROPERTIES LINK_DEPENDS
            ${MICROKERNEL_USER_DYN_LINKER_SCRIPT})
        target_link_libraries(${name} PRIVATE "${LIBC_SO_OUT}")
        add_dependencies(${name} libcso crt0)
        set(sysroot_out "${SYSROOT_DIR}/${sysroot_subdir}/${boot_name}")
        add_custom_command(
            OUTPUT "${sysroot_out}"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${SYSROOT_DIR}/${sysroot_subdir}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:${name}>" "${sysroot_out}"
            DEPENDS ${name}
            COMMENT "Stage 2: installing ${boot_name} into sysroot"
            VERBATIM
        )
        add_custom_target(${name}_install ALL DEPENDS "${sysroot_out}")
        set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_MODULES
            "${boot_name}=$<TARGET_FILE:${name}>")
        set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_DEPS
            ${name} ${name}_install)
    endfunction()

    microkernel_add_tzcode_executable(tzcode_zic zic usr/sbin
        "${TZCODE_SRC}/zic.c")
    microkernel_add_tzcode_executable(tzcode_zdump zdump usr/bin
        "${TZCODE_SRC}/zdump.c"
        "${TZCODE_SRC}/localtime.c"
        "${TZCODE_SRC}/strftime.c")

    # Generate tzselect from tzselect.ksh with Ring 3 /bin/sh and /bin/awk.
    set(TZSELECT_OUT "${SYSROOT_DIR}/usr/bin/tzselect")
    string(CONCAT TZSELECT_PY
        "import re, sys; from pathlib import Path; "
        "s = Path(sys.argv[1]).read_text('utf-8'); "
        "s = s.replace('#!/bin/bash', '#!/bin/sh', 1); "
        "s = re.sub(r'(AWK)=[^}\\n]*', r\"\\1='/bin/awk'\", s); "
        "s = re.sub(r'(PKGVERSION)=[^\\n]*', r\"\\1='(tzcode) '\", s); "
        "s = re.sub(r'(REPORT_BUGS_TO)=[^\\n]*', "
        "r\"\\1='tz@iana.org'\", s); "
        "s = re.sub(r'(TZDIR)=[^}\\n]*', "
        "r\"\\1='/usr/share/zoneinfo'\", s); "
        "s = re.sub(r'(TZVERSION)=[^\\n]*', "
        "f\"TZVERSION='{sys.argv[3]}'\", s); "
        "p = Path(sys.argv[2]); p.write_text(s, 'utf-8'); p.chmod(0o755)"
    )
    add_custom_command(
        OUTPUT "${TZSELECT_OUT}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SYSROOT_DIR}/usr/bin"
        COMMAND "${Python3_EXECUTABLE}" -c "${TZSELECT_PY}"
                "${TZCODE_SRC}/tzselect.ksh" "${TZSELECT_OUT}"
                "${TZCODE_VERSION}"
        DEPENDS "${TZCODE_SRC}/tzselect.ksh" "${TZCODE_SRC}/version"
        COMMENT "Stage 2: generating tzselect into sysroot"
        VERBATIM
    )
    add_custom_target(tzcode_tzselect ALL DEPENDS "${TZSELECT_OUT}")
    set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_MODULES
        "tzselect=${TZSELECT_OUT}")
    set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_DEPS tzcode_tzselect)

    # Install real IANA tzdata source and table files into sysroot and boot.tar
    # for TZif compilation, zdump verification, and tzselect lookups.
    set(TZDATA_INSTALL_FILES
        etcetera
        australasia
        iso3166.tab
        zone1970.tab
    )
    set(TZDATA_SYSROOT_OUTS "")
    foreach(tzf IN LISTS TZDATA_INSTALL_FILES)
        set(dst "${SYSROOT_DIR}/usr/share/zoneinfo/${tzf}")
        add_custom_command(
            OUTPUT "${dst}"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${SYSROOT_DIR}/usr/share/zoneinfo"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${TZDATA_SRC}/${tzf}" "${dst}"
            DEPENDS "${TZDATA_SRC}/${tzf}"
            COMMENT "Stage 2: installing tzdata ${tzf} into sysroot"
            VERBATIM
        )
        list(APPEND TZDATA_SYSROOT_OUTS "${dst}")
        set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_MODULES
            "${tzf}=${dst}")
    endforeach()
    add_custom_target(tzcode_tzdata ALL DEPENDS ${TZDATA_SYSROOT_OUTS})
    set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_DEPS tzcode_tzdata)
endif()
