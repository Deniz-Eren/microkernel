##
# @file     tar.cmake
# @brief    Download and cross-compile libarchive bsdtar for Ring 3
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

# need:: COMP_LIBC_0039

option(MICROKERNEL_BUILD_TAR
    "Download and cross-compile libarchive bsdtar into the Ring 3 sysroot" ON)

if(MICROKERNEL_BUILD_TAR)
    if(NOT MICROKERNEL_BUILD_ZLIB)
        message(FATAL_ERROR
            "MICROKERNEL_BUILD_TAR requires MICROKERNEL_BUILD_ZLIB=ON")
    endif()
    include(FetchContent)

    set(MICROKERNEL_TAR_VERSION "3.8.2")
    string(CONCAT MICROKERNEL_TAR_DEFAULT_URL
        "https://github.com/libarchive/libarchive/releases/download/"
        "v${MICROKERNEL_TAR_VERSION}/libarchive-${MICROKERNEL_TAR_VERSION}"
        ".tar.gz")
    set(MICROKERNEL_TAR_URL
        "${MICROKERNEL_TAR_DEFAULT_URL}"
        CACHE STRING "libarchive release archive URL")
    string(CONCAT MICROKERNEL_TAR_DEFAULT_SHA256
        "5f2d3c2fde8dc44583a61165549dc50ba8a"
        "37c5947c90fc02c8e5ce7f1cfb80d")
    set(MICROKERNEL_TAR_SHA256
        "${MICROKERNEL_TAR_DEFAULT_SHA256}"
        CACHE STRING "libarchive release archive SHA-256")

    FetchContent_Declare(
        tar
        URL "${MICROKERNEL_TAR_URL}"
        URL_HASH "SHA256=${MICROKERNEL_TAR_SHA256}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_GetProperties(tar)
    if(NOT tar_POPULATED)
        FetchContent_Populate(tar)
    endif()

    set(TAR_SRC "${tar_SOURCE_DIR}")
    set(TAR_GEN "${CMAKE_BINARY_DIR}/tar")
    file(MAKE_DIRECTORY "${TAR_GEN}")
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/tar_config.h"
        "${TAR_GEN}/config.h"
        COPYONLY
    )

    # bsdtar objects link into one ET_DYN PIE executable against the shared
    # sysroot libraries, so every object compiles with -fPIC (ADR_0059).
    # HAVE_CONFIG_H routes archive_platform.h and bsdtar_platform.h to the
    # hand-written config.h in TAR_GEN (cmake/tar_config.h). Only zlib is
    # enabled; .tar.gz needs libz.so.1, all other filters stay disabled.
    set(TAR_CFLAGS
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
        -DHAVE_CONFIG_H
        -D_GNU_SOURCE
        -D_DEFAULT_SOURCE
        -DPLATFORM_CONFIG_H="config.h"
    )

    set(TAR_INC_DIRS
        "${TAR_GEN}"
        "${TAR_SRC}"
        "${TAR_SRC}/libarchive"
        "${TAR_SRC}/libarchive_fe"
        "${SYSROOT_DIR}/usr/include"
        "${CLANG_RESOURCE_DIR}/include"
    )

    set(TAR_LIBARCHIVE_SRCS
        archive_acl.c
        archive_check_magic.c
        archive_cmdline.c
        archive_cryptor.c
        archive_digest.c
        archive_entry.c
        archive_entry_copy_stat.c
        archive_entry_link_resolver.c
        archive_entry_sparse.c
        archive_entry_stat.c
        archive_entry_strmode.c
        archive_entry_xattr.c
        archive_hmac.c
        archive_match.c
        archive_options.c
        archive_pack_dev.c
        archive_parse_date.c
        archive_pathmatch.c
        archive_ppmd8.c
        archive_ppmd7.c
        archive_random.c
        archive_rb.c
        archive_read.c
        archive_read_add_passphrase.c
        archive_read_append_filter.c
        archive_read_data_into_fd.c
        archive_read_disk_entry_from_file.c
        archive_read_disk_posix.c
        archive_read_disk_set_standard_lookup.c
        archive_read_extract.c
        archive_read_extract2.c
        archive_read_open_fd.c
        archive_read_open_file.c
        archive_read_open_filename.c
        archive_read_open_memory.c
        archive_read_set_format.c
        archive_read_set_options.c
        archive_read_support_filter_all.c
        archive_read_support_filter_by_code.c
        archive_read_support_filter_bzip2.c
        archive_read_support_filter_compress.c
        archive_read_support_filter_gzip.c
        archive_read_support_filter_grzip.c
        archive_read_support_filter_lrzip.c
        archive_read_support_filter_lz4.c
        archive_read_support_filter_lzop.c
        archive_read_support_filter_none.c
        archive_read_support_filter_program.c
        archive_read_support_filter_rpm.c
        archive_read_support_filter_uu.c
        archive_read_support_filter_xz.c
        archive_read_support_filter_zstd.c
        archive_read_support_format_7zip.c
        archive_read_support_format_all.c
        archive_read_support_format_ar.c
        archive_read_support_format_by_code.c
        archive_read_support_format_cab.c
        archive_read_support_format_cpio.c
        archive_read_support_format_empty.c
        archive_read_support_format_iso9660.c
        archive_read_support_format_lha.c
        archive_read_support_format_mtree.c
        archive_read_support_format_rar.c
        archive_read_support_format_rar5.c
        archive_read_support_format_raw.c
        archive_read_support_format_tar.c
        archive_read_support_format_warc.c
        archive_read_support_format_xar.c
        archive_read_support_format_zip.c
        archive_string.c
        archive_string_sprintf.c
        archive_time.c
        archive_util.c
        archive_version_details.c
        archive_virtual.c
        archive_write.c
        archive_write_disk_posix.c
        archive_write_disk_set_standard_lookup.c
        archive_write_open_fd.c
        archive_write_open_file.c
        archive_write_open_filename.c
        archive_write_open_memory.c
        archive_write_add_filter.c
        archive_write_add_filter_b64encode.c
        archive_write_add_filter_by_name.c
        archive_write_add_filter_bzip2.c
        archive_write_add_filter_compress.c
        archive_write_add_filter_grzip.c
        archive_write_add_filter_gzip.c
        archive_write_add_filter_lrzip.c
        archive_write_add_filter_lz4.c
        archive_write_add_filter_lzop.c
        archive_write_add_filter_none.c
        archive_write_add_filter_program.c
        archive_write_add_filter_uuencode.c
        archive_write_add_filter_xz.c
        archive_write_add_filter_zstd.c
        archive_write_set_format.c
        archive_write_set_format_7zip.c
        archive_write_set_format_ar.c
        archive_write_set_format_by_name.c
        archive_write_set_format_cpio.c
        archive_write_set_format_cpio_binary.c
        archive_write_set_format_cpio_newc.c
        archive_write_set_format_cpio_odc.c
        archive_write_set_format_filter_by_ext.c
        archive_write_set_format_gnutar.c
        archive_write_set_format_iso9660.c
        archive_write_set_format_mtree.c
        archive_write_set_format_pax.c
        archive_write_set_format_raw.c
        archive_write_set_format_shar.c
        archive_write_set_format_ustar.c
        archive_write_set_format_v7tar.c
        archive_write_set_format_warc.c
        archive_write_set_format_xar.c
        archive_write_set_format_zip.c
        archive_write_set_options.c
        archive_write_set_passphrase.c
        archive_blake2s_ref.c
        archive_blake2sp_ref.c
        filter_fork_posix.c
        xxhash.c
    )
    list(TRANSFORM TAR_LIBARCHIVE_SRCS PREPEND "${TAR_SRC}/libarchive/")

    set(TAR_BSDTAR_SRCS
        bsdtar.c
        cmdline.c
        creation_set.c
        read.c
        subst.c
        util.c
        write.c
        ../libarchive_fe/lafe_err.c
        ../libarchive_fe/line_reader.c
        ../libarchive_fe/passphrase.c
    )
    list(TRANSFORM TAR_BSDTAR_SRCS PREPEND "${TAR_SRC}/tar/")

    # Dynamic Ring 3 executable: ET_DYN PIE with PT_INTERP=/boot/ld.so linked
    # against the shared sysroot libraries through the user-dyn.ld script.
    # Linking against the shared objects records DT_NEEDED libz.so.1 and
    # libc.so; /boot/ld.so resolves them from boot.tar at load time (ADR_0059).
    # Undefined libc references stay undefined at link time.
    add_executable(bsdtar ${TAR_LIBARCHIVE_SRCS} ${TAR_BSDTAR_SRCS})
    target_compile_options(bsdtar PRIVATE ${TAR_CFLAGS})
    target_include_directories(bsdtar SYSTEM PRIVATE ${TAR_INC_DIRS})
    target_link_options(bsdtar PRIVATE
        -pie
        --dynamic-linker=/boot/ld.so
        -T ${MICROKERNEL_USER_DYN_LINKER_SCRIPT}
        --build-id=none
        ${SYSROOT_DIR}/usr/lib/crt0.o
    )
    set_target_properties(bsdtar PROPERTIES LINK_DEPENDS
        ${MICROKERNEL_USER_DYN_LINKER_SCRIPT})
    target_link_libraries(bsdtar PRIVATE
        "${ZLIB_SO_OUT}" "${LIBC_SO_OUT}")
    add_dependencies(bsdtar libcso zlibso crt0 install_zlib_headers)

    set(TAR_SYSROOT_OUT "${SYSROOT_DIR}/usr/bin/tar")
    add_custom_command(
        OUTPUT "${TAR_SYSROOT_OUT}"
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "${SYSROOT_DIR}/usr/bin"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:bsdtar>" "${TAR_SYSROOT_OUT}"
        DEPENDS bsdtar
        COMMENT "Stage 2: installing tar into sysroot"
        VERBATIM
    )
    add_custom_target(tar_install ALL DEPENDS "${TAR_SYSROOT_OUT}")
    set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_MODULES
        "tar=$<TARGET_FILE:bsdtar>")
    set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_DEPS
        bsdtar tar_install)
endif()
