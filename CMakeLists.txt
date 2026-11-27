##
# @file     CMakeLists.txt
# @brief    Build the kernel, the C library, user programs, and the boot ISO
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.20)
project(microkernel C ASM)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(MICROKERNEL_ARCH "x86-64" CACHE STRING "Target 64-bit platform (x86-64)")
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/cmake/arch/${MICROKERNEL_ARCH}.cmake")
    message(FATAL_ERROR
        "Unsupported MICROKERNEL_ARCH='${MICROKERNEL_ARCH}'. "
        "Currently supported platform: x86-64 (future arm64 planned).")
endif()
include("${CMAKE_SOURCE_DIR}/cmake/arch/${MICROKERNEL_ARCH}.cmake")

option(MICROKERNEL_COVERAGE "Enable LLVM source-based code coverage" OFF)
if(MICROKERNEL_COVERAGE)
    set(SYSROOT_DIR ${CMAKE_BINARY_DIR}/sysroot)
    set(COV_INSTR_FLAGS
        -fprofile-instr-generate
        -fcoverage-mapping
        -DMICROKERNEL_COVERAGE=1
    )
else()
    set(SYSROOT_DIR ${CMAKE_SOURCE_DIR}/sysroot)
    set(COV_INSTR_FLAGS "")
endif()

# Clang provides freestanding headers (stdint.h, stddef.h, stdarg.h, ...) in its
# resource dir; we pass -nostdinc and re-add exactly that directory.
execute_process(
    COMMAND ${CMAKE_C_COMPILER} -print-resource-dir
    OUTPUT_VARIABLE CLANG_RESOURCE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Link directly with ld.lld (no GCC driver involvement). <LINK_LIBRARIES> is
# empty for the kernel and carries libc.a for userland targets.
set(CMAKE_C_LINK_EXECUTABLE
    "${CMAKE_LINKER} <LINK_FLAGS> <OBJECTS> <LINK_LIBRARIES> -o <TARGET>")

#------------------------------------------------------------------------------
# Stage 1: install public headers into the sysroot
#------------------------------------------------------------------------------
file(GLOB_RECURSE PUBLIC_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/libc/include/*.h
    ${CMAKE_SOURCE_DIR}/libc/arch/${MICROKERNEL_ARCH}/include/*.h)
# Freestanding compiler headers (stdint.h, stddef.h, stdarg.h, ...) are vendored
# into the sysroot so every component compiles against it uniformly.
add_custom_target(install_headers
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SYSROOT_DIR}/usr/include
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CLANG_RESOURCE_DIR}/include ${SYSROOT_DIR}/usr/include
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/libc/include ${SYSROOT_DIR}/usr/include
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/libc/arch/${MICROKERNEL_ARCH}/include
            ${SYSROOT_DIR}/usr/include
    DEPENDS ${PUBLIC_HEADERS}
    COMMENT "Stage 1: installing ${MICROKERNEL_ARCH} headers into sysroot"
)

#------------------------------------------------------------------------------
# Stage 2: cross-compile libc + crt0 against the sysroot
#------------------------------------------------------------------------------
set(USER_CFLAGS
    -ffreestanding
    -nostdlib
    -nostdinc
    -fno-builtin
    -fno-stack-protector
    -fno-pic
    -fno-pie
    -fno-asynchronous-unwind-tables
    ${MICROKERNEL_USER_ARCH_CFLAGS}
    -Wall
    -Wextra
    -Werror
)
# CMake dedups identical "-isystem" tokens; add include dirs per-target instead.

file(GLOB LIBC_SRCS CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/libc/src/*.c
    ${CMAKE_SOURCE_DIR}/libc/arch/${MICROKERNEL_ARCH}/*.c)
if(MICROKERNEL_COVERAGE)
    foreach(src IN LISTS LIBC_SRCS)
        get_filename_component(fname "${src}" NAME)
        if(fname STREQUAL "cov_runtime.c")
            set_source_files_properties("${src}" PROPERTIES
                COMPILE_OPTIONS "-DMICROKERNEL_COVERAGE=1")
        else()
            set_source_files_properties("${src}" PROPERTIES
                COMPILE_OPTIONS "${COV_INSTR_FLAGS}")
        endif()
    endforeach()
endif()
add_library(c STATIC ${LIBC_SRCS} ${MICROKERNEL_LIBC_ASM_SRCS})
target_compile_options(c PRIVATE ${USER_CFLAGS})
target_include_directories(c SYSTEM PRIVATE
    ${SYSROOT_DIR}/usr/include
    ${CLANG_RESOURCE_DIR}/include
)
set_target_properties(c PROPERTIES
    ARCHIVE_OUTPUT_NAME "c"
    ARCHIVE_OUTPUT_DIRECTORY ${SYSROOT_DIR}/usr/lib
    SUFFIX ".a"
)
add_dependencies(c install_headers)

add_custom_command(
    OUTPUT ${SYSROOT_DIR}/usr/lib/crt0.o
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SYSROOT_DIR}/usr/lib
    COMMAND ${CMAKE_C_COMPILER} ${USER_CFLAGS}
            -target ${MICROKERNEL_TARGET_TRIPLE}
            -isystem ${SYSROOT_DIR}/usr/include
            -c ${MICROKERNEL_CRT0_SRC}
            -o ${SYSROOT_DIR}/usr/lib/crt0.o
    DEPENDS ${MICROKERNEL_CRT0_SRC} install_headers
    COMMENT "Stage 2: compiling crt0.o (${MICROKERNEL_ARCH})"
    VERBATIM
)
add_custom_target(crt0 DEPENDS ${SYSROOT_DIR}/usr/lib/crt0.o)
add_dependencies(c crt0)

#------------------------------------------------------------------------------
# Stage 2: shared libc.so (-fPIC) alongside static libc.a. Dynamic Ring 3
# executables link against libc.so and load /boot/ld.so; static executables keep
# linking libc.a. In coverage builds the instrumented libc.a objects carry the
# profile runtime, so libc.so stays linkable with the same source-file
# properties (cov_runtime.c provides the freestanding stubs).
#------------------------------------------------------------------------------
set(LIBC_SO_OUT ${CMAKE_BINARY_DIR}/libc.so)
add_library(libcso_obj OBJECT ${LIBC_SRCS} ${MICROKERNEL_LIBC_ASM_SRCS})
set(LIBCSO_CFLAGS ${USER_CFLAGS})
list(REMOVE_ITEM LIBCSO_CFLAGS -fno-pic -fno-pie)
list(APPEND LIBCSO_CFLAGS -fPIC)
target_compile_options(libcso_obj PRIVATE ${LIBCSO_CFLAGS})
target_include_directories(libcso_obj SYSTEM PRIVATE
    ${SYSROOT_DIR}/usr/include
    ${CLANG_RESOURCE_DIR}/include
)
add_dependencies(libcso_obj install_headers)

add_custom_command(
    OUTPUT ${LIBC_SO_OUT}
    COMMAND ${CMAKE_LINKER} -shared -soname libc.so
            -z separate-code
            --hash-style=both
            --build-id=none
            $<TARGET_OBJECTS:libcso_obj>
            -o ${LIBC_SO_OUT}
    DEPENDS libcso_obj $<TARGET_OBJECTS:libcso_obj>
    COMMENT "Stage 2: linking shared libc.so"
    VERBATIM
    COMMAND_EXPAND_LISTS
)
add_custom_target(libcso ALL DEPENDS ${LIBC_SO_OUT})
add_custom_command(
    OUTPUT ${SYSROOT_DIR}/usr/lib/libc.so
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${LIBC_SO_OUT} ${SYSROOT_DIR}/usr/lib/libc.so
    DEPENDS ${LIBC_SO_OUT}
    COMMENT "Stage 2: installing libc.so into the sysroot"
    VERBATIM
)
add_custom_target(libcso_install DEPENDS ${SYSROOT_DIR}/usr/lib/libc.so)
add_dependencies(libcso libcso_install)
set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_MODULES
    "libc.so=${LIBC_SO_OUT}")
set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_DEPS libcso)

include(cmake/zlib.cmake)
include(cmake/openssl.cmake)
include(cmake/tzcode.cmake)

#------------------------------------------------------------------------------
# Stage 3: Ring 3 servers and test applications
#------------------------------------------------------------------------------
set(CMAKE_C_LINK_EXECUTABLE
    "${CMAKE_LINKER} <LINK_FLAGS> <OBJECTS> <LINK_LIBRARIES> -o <TARGET>")

function(microkernel_add_user_executable name)
    set(boot_name "${name}")
    set(srcs ${ARGN})
    list(GET srcs 0 first_arg)
    if(first_arg STREQUAL "BOOT_NAME")
        list(GET srcs 1 boot_name)
        list(SUBLIST srcs 2 -1 srcs)
    endif()
    add_executable(${name} ${srcs})
    target_compile_options(${name} PRIVATE ${USER_CFLAGS})
    if(MICROKERNEL_COVERAGE)
        target_compile_options(${name} PRIVATE ${COV_INSTR_FLAGS})
    endif()
    target_include_directories(${name} SYSTEM PRIVATE
        ${SYSROOT_DIR}/usr/include
        ${CLANG_RESOURCE_DIR}/include
    )
    target_link_options(${name} PRIVATE
        -static
        --no-pie
        -T ${MICROKERNEL_USER_LINKER_SCRIPT}
        -z noseparate-code
        --build-id=none
        ${SYSROOT_DIR}/usr/lib/crt0.o
    )
    set_target_properties(${name} PROPERTIES LINK_DEPENDS
        ${MICROKERNEL_USER_LINKER_SCRIPT})
    target_link_libraries(${name} PRIVATE c)
    add_dependencies(${name} c)
    set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_MODULES
        "${boot_name}=$<TARGET_FILE:${name}>")
    set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_DEPS ${name})
endfunction()

function(microkernel_add_user_shared_library name)
    set(boot_name "${name}")
    set(srcs ${ARGN})
    list(GET srcs 0 first_arg)
    if(first_arg STREQUAL "BOOT_NAME")
        list(GET srcs 1 boot_name)
        list(SUBLIST srcs 2 -1 srcs)
    endif()
    set(obj_target "${name}_obj")
    add_library(${obj_target} OBJECT ${srcs})
    set(pic_cflags ${USER_CFLAGS})
    list(REMOVE_ITEM pic_cflags -fno-pic -fno-pie)
    list(APPEND pic_cflags -fPIC)
    target_compile_options(${obj_target} PRIVATE ${pic_cflags})
    target_include_directories(${obj_target} SYSTEM PRIVATE
        ${SYSROOT_DIR}/usr/include
        ${CLANG_RESOURCE_DIR}/include
    )
    add_dependencies(${obj_target} install_headers)
    set(out_so "${CMAKE_CURRENT_BINARY_DIR}/${boot_name}")
    add_custom_command(
        OUTPUT "${out_so}"
        COMMAND ${CMAKE_LINKER}
                -shared
                -soname ${boot_name}
                -z separate-code
                --hash-style=both
                --build-id=none
                $<TARGET_OBJECTS:${obj_target}>
                -o "${out_so}"
        DEPENDS ${obj_target} $<TARGET_OBJECTS:${obj_target}>
        COMMENT "Stage 3: linking Ring 3 shared object ${boot_name}"
        VERBATIM
        COMMAND_EXPAND_LISTS
    )
    add_custom_target(${name} ALL DEPENDS "${out_so}")
    set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_MODULES
        "${boot_name}=${out_so}")
    set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_DEPS ${name})
endfunction()

# Dynamic Ring 3 executables: ET_DYN PIE with PT_INTERP=/boot/ld.so linked
# against the shared libc.so through the user-dyn.ld script. Dynamic executables
# never carry coverage instrumentation: the profile runtime lives in static
# libc.a only.
function(microkernel_add_user_dynamic_executable name)
    set(boot_name "${name}")
    set(srcs ${ARGN})
    list(GET srcs 0 first_arg)
    if(first_arg STREQUAL "BOOT_NAME")
        list(GET srcs 1 boot_name)
        list(SUBLIST srcs 2 -1 srcs)
    endif()
    add_executable(${name} ${srcs})
    set(dyn_cflags ${USER_CFLAGS})
    list(REMOVE_ITEM dyn_cflags -fno-pic -fno-pie)
    list(APPEND dyn_cflags -fPIC)
    target_compile_options(${name} PRIVATE ${dyn_cflags})
    target_include_directories(${name} SYSTEM PRIVATE
        ${SYSROOT_DIR}/usr/include
        ${CLANG_RESOURCE_DIR}/include
    )
    target_link_options(${name} PRIVATE
        -pie
        --dynamic-linker=/boot/ld.so
        --export-dynamic
        -T ${MICROKERNEL_USER_DYN_LINKER_SCRIPT}
        --build-id=none
        ${SYSROOT_DIR}/usr/lib/crt0.o
    )
    set_target_properties(${name} PROPERTIES LINK_DEPENDS
        ${MICROKERNEL_USER_DYN_LINKER_SCRIPT})
    target_link_libraries(${name} PRIVATE ${LIBC_SO_OUT})
    add_dependencies(${name} libcso crt0)
    set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_MODULES
        "${boot_name}=$<TARGET_FILE:${name}>")
    set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_DEPS ${name})
endfunction()

#------------------------------------------------------------------------------
# Stage 2: freestanding Ring 3 dynamic linker (ld.so)
#------------------------------------------------------------------------------
set(LDSO_DIR ${CMAKE_SOURCE_DIR}/libc/ldso)
add_library(ldso_obj OBJECT ${LDSO_DIR}/ldso.c)
set(LDSO_CFLAGS ${USER_CFLAGS})
list(REMOVE_ITEM LDSO_CFLAGS -fno-pic -fno-pie)
list(APPEND LDSO_CFLAGS -fPIC -fvisibility=hidden)
target_compile_options(ldso_obj PRIVATE ${LDSO_CFLAGS})
target_include_directories(ldso_obj SYSTEM PRIVATE
    ${SYSROOT_DIR}/usr/include
    ${CLANG_RESOURCE_DIR}/include
)
add_dependencies(ldso_obj install_headers)

set(LDSO_START_O ${CMAKE_BINARY_DIR}/ldso_start.o)
add_custom_command(
    OUTPUT ${LDSO_START_O}
    COMMAND ${CMAKE_C_COMPILER} ${USER_CFLAGS}
            -target ${MICROKERNEL_TARGET_TRIPLE}
            -c ${LDSO_DIR}/start.S -o ${LDSO_START_O}
    DEPENDS ${LDSO_DIR}/start.S
    COMMENT "Stage 2: compiling ld.so entry (start.S)"
    VERBATIM
)

set(LDSO_OUT ${CMAKE_BINARY_DIR}/ld.so)
add_custom_command(
    OUTPUT ${LDSO_OUT}
    COMMAND ${CMAKE_LINKER} -shared -e _start
            -z separate-code
            --hash-style=both
            --build-id=none
            ${LDSO_START_O} $<TARGET_OBJECTS:ldso_obj>
            -o ${LDSO_OUT}
    DEPENDS ${LDSO_START_O} ldso_obj $<TARGET_OBJECTS:ldso_obj>
    COMMENT "Stage 2: linking Ring 3 dynamic linker ld.so"
    VERBATIM
    COMMAND_EXPAND_LISTS
)
add_custom_target(ldso ALL DEPENDS ${LDSO_OUT})
set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_MODULES
    "ld.so=${LDSO_OUT}")
# /usr/bin/ldd is the ld.so image itself; the bootfs entry "ldd" carries the
# same file so execve("ldd") and execve("/boot/ldd") run the ldd trace mode.
set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_MODULES
    "ldd=${LDSO_OUT}")
set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_DEPS ldso)

add_subdirectory(servers/sysproc)
add_subdirectory(servers/pipeserv)
add_subdirectory(servers/vfsserv)
add_subdirectory(servers/ttyserv)
add_subdirectory(servers/sockserv)
add_subdirectory(servers/ipcserv)
add_subdirectory(servers/rtcdrv)
add_subdirectory(servers/blkdrv)
add_subdirectory(servers/netserv)
add_subdirectory(bin)
include(cmake/openssh.cmake)
add_subdirectory(tests/ipccli)
add_subdirectory(tests/sigtest)
add_subdirectory(tests/proctest)
add_subdirectory(tests/drvtest)
add_subdirectory(tests/timetest)
add_subdirectory(tests/pthreadtest)
add_subdirectory(tests/forkpipetest)
add_subdirectory(tests/aiotest)
add_subdirectory(tests/canceltest)
add_subdirectory(tests/libctest)
add_subdirectory(tests/heaptest)
add_subdirectory(tests/vfstest)
add_subdirectory(tests/shmtest)
add_subdirectory(tests/ttytest)
add_subdirectory(tests/stdiotest)
add_subdirectory(tests/socktest)
add_subdirectory(tests/shtest)
add_subdirectory(tests/utilstest)
add_subdirectory(tests/jobtest)
add_subdirectory(tests/simdtest)
add_subdirectory(tests/mathtest)
add_subdirectory(tests/blktest)
add_subdirectory(tests/nettest)
add_subdirectory(tests/ext2test)
add_subdirectory(tests/zlibtest)
add_subdirectory(tests/openssltest)
add_subdirectory(tests/tztest)
add_subdirectory(tests/dlsoctest)
add_subdirectory(tests/dynlibctest)
add_subdirectory(tests/lddtest)
add_subdirectory(tests/smptest)
add_subdirectory(tests/stresstest)

#------------------------------------------------------------------------------
# boot.tar (ustar archive of Ring 3 servers/apps)
#------------------------------------------------------------------------------
get_property(MICROKERNEL_BOOT_MODULES GLOBAL PROPERTY MICROKERNEL_BOOT_MODULES)
get_property(MICROKERNEL_BOOT_DEPS GLOBAL PROPERTY MICROKERNEL_BOOT_DEPS)
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/boot.tar
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/mk_boot_tar.py
            --output ${CMAKE_BINARY_DIR}/boot.tar ${MICROKERNEL_BOOT_MODULES}
    DEPENDS ${CMAKE_SOURCE_DIR}/scripts/mk_boot_tar.py ${MICROKERNEL_BOOT_DEPS}
    COMMENT "Stage 3: packing boot.tar (ustar bootfs)"
    VERBATIM
)
add_custom_target(boot_tar DEPENDS ${CMAKE_BINARY_DIR}/boot.tar)

#------------------------------------------------------------------------------
# Stage 4: Ring 0 kernel
#------------------------------------------------------------------------------
set(KERNEL_CFLAGS
    -ffreestanding
    -nostdlib
    -nostdinc
    -fno-builtin
    -fno-stack-protector
    -fno-pic
    -fno-pie
    -fno-asynchronous-unwind-tables
    ${MICROKERNEL_KERNEL_ARCH_CFLAGS}
    -Wall
    -Wextra
    -Werror
    -isystem ${SYSROOT_DIR}/usr/include
)

file(GLOB KERNEL_CORE_SRCS CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/kernel/main.c
    ${CMAKE_SOURCE_DIR}/kernel/core/*.c
    ${CMAKE_SOURCE_DIR}/kernel/lib/*.c
    ${CMAKE_SOURCE_DIR}/kernel/arch/${MICROKERNEL_ARCH}/*.c
)
if(MICROKERNEL_COVERAGE)
    foreach(src IN LISTS KERNEL_CORE_SRCS)
        get_filename_component(fname "${src}" NAME)
        if(fname STREQUAL "cov.c")
            set_source_files_properties("${src}" PROPERTIES
                COMPILE_OPTIONS "-DMICROKERNEL_COVERAGE=1")
        else()
            set_source_files_properties("${src}" PROPERTIES
                COMPILE_OPTIONS "${COV_INSTR_FLAGS}")
        endif()
    endforeach()
endif()

add_executable(kernel.elf
    ${KERNEL_CORE_SRCS}
    ${MICROKERNEL_KERNEL_ASM_SRCS}
)
target_compile_options(kernel.elf PRIVATE ${KERNEL_CFLAGS})
target_include_directories(kernel.elf PRIVATE
    ${CMAKE_SOURCE_DIR}/kernel
    ${CMAKE_SOURCE_DIR}/kernel/include
    ${CMAKE_SOURCE_DIR}/kernel/arch/${MICROKERNEL_ARCH}
    ${CMAKE_SOURCE_DIR}/libc/include
    ${CMAKE_SOURCE_DIR}/libc/arch/${MICROKERNEL_ARCH}/include
)

target_link_options(kernel.elf PRIVATE
    -static
    --no-pie
    -T ${MICROKERNEL_KERNEL_LINKER_SCRIPT}
    --build-id=none
    -z max-page-size=0x1000
)
set_target_properties(kernel.elf PROPERTIES
    LINK_DEPENDS ${MICROKERNEL_KERNEL_LINKER_SCRIPT}
)
add_dependencies(kernel.elf install_headers)

#------------------------------------------------------------------------------
# Stage 4: bootable ISO via grub-mkrescue
#------------------------------------------------------------------------------
add_custom_target(iso ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/iso/boot/grub
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/iso/boot/grub/grub.cfg
            ${CMAKE_BINARY_DIR}/iso/boot/grub/grub.cfg
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/kernel.elf
            ${CMAKE_BINARY_DIR}/iso/boot/kernel.elf
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/boot.tar
            ${CMAKE_BINARY_DIR}/iso/boot/boot.tar
    COMMAND grub-mkrescue -o ${CMAKE_BINARY_DIR}/microkernel.iso
            ${CMAKE_BINARY_DIR}/iso
    DEPENDS kernel.elf boot_tar
    COMMENT "Stage 4: building bootable ISO"
    VERBATIM
)

#------------------------------------------------------------------------------
# Documentation and traceability (Sphinx, MyST-Parser, Sphinx-Needs, CTest)
#------------------------------------------------------------------------------
if(NOT MICROKERNEL_COVERAGE)
    add_subdirectory(docs)
endif()
enable_testing()

add_test(
    NAME posix.headers
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/posix/test_headers.py"
        --cc "${CMAKE_C_COMPILER}"
        --target "${MICROKERNEL_TARGET_TRIPLE}"
        --clang-include "${CLANG_RESOURCE_DIR}/include"
        --sysroot "${SYSROOT_DIR}/usr/include"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(
    posix.headers
    PROPERTIES LABELS "posix;libc"
)

add_test(
    NAME posix.symbol_diff
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/posix/test_symbol_diff.py"
        --cc "${CMAKE_C_COMPILER}"
        --target "${MICROKERNEL_TARGET_TRIPLE}"
        --clang-include "${CLANG_RESOURCE_DIR}/include"
        --sysroot "${SYSROOT_DIR}/usr/include"
        --nm "${CMAKE_NM}"
        --libc "${SYSROOT_DIR}/usr/lib/libc.a"
        --manifest "${CMAKE_SOURCE_DIR}/tests/posix/issue8-manifest.tsv"
        --bin-cmake "${CMAKE_SOURCE_DIR}/bin/CMakeLists.txt"
        --boot-tar "${CMAKE_BINARY_DIR}/boot.tar"
        --report "${CMAKE_BINARY_DIR}/posix-symbol-diff.txt"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(
    posix.symbol_diff
    PROPERTIES LABELS "posix;libc"
)

add_test(
    NAME posix.async_signal_safe
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/posix/test_async_signal_safe.py"
        --objdump "${CMAKE_OBJDUMP}"
        --libc "${SYSROOT_DIR}/usr/lib/libc.a"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(
    posix.async_signal_safe
    PROPERTIES LABELS "posix;sig"
)

add_test(
    NAME posix.cancellation
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/posix/test_cancellation.py"
        --objdump "${CMAKE_OBJDUMP}"
        --libc "${SYSROOT_DIR}/usr/lib/libc.a"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(
    posix.cancellation
    PROPERTIES LABELS "posix;libc"
)

add_test(
    NAME coverage.unit
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/coverage/test_generate_report.py"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(
    coverage.unit
    PROPERTIES LABELS "docs;coverage"
)

add_test(
    NAME arch.microkernel_boundary
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/arch/test_microkernel_boundary.py"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(
    arch.microkernel_boundary
    PROPERTIES LABELS "arch;docs"
)

if(MICROKERNEL_ARCH STREQUAL "x86-64" AND NOT MICROKERNEL_COVERAGE)
    add_test(
        NAME boot.x86_64_v3
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tests/boot/test_x86_64_v3.py"
            --iso "${CMAKE_BINARY_DIR}/microkernel.iso"
            --kernel "${CMAKE_BINARY_DIR}/kernel.elf"
            --libc "${SYSROOT_DIR}/usr/lib/libc.a"
            --simdtest "$<TARGET_FILE:simdtest>"
            --objdump "${CMAKE_OBJDUMP}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(
        boot.x86_64_v3
        PROPERTIES LABELS "boot;arch"
    )
    add_test(
        NAME ipc.blk_persist
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tests/boot/test_blk_persist.py"
            --iso "${CMAKE_BINARY_DIR}/microkernel.iso"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(
        ipc.blk_persist
        PROPERTIES LABELS "ipc;blk"
    )
    add_test(
        NAME ipc.vfs_persist
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tests/boot/test_ext2_persist.py"
            --iso "${CMAKE_BINARY_DIR}/microkernel.iso"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(
        ipc.vfs_persist
        PROPERTIES LABELS "ipc;vfs"
    )
    add_test(
        NAME proc.smp_cores
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tests/boot/test_smp_cores.py"
            --iso "${CMAKE_BINARY_DIR}/microkernel.iso"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(
        proc.smp_cores
        PROPERTIES LABELS "proc;smp"
    )
    add_test(
        NAME ipc.net_hostfwd
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tests/boot/test_net_hostfwd.py"
            --build-dir "${CMAKE_BINARY_DIR}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(
        ipc.net_hostfwd
        PROPERTIES LABELS "ipc;net"
    )
    add_test(
        NAME ipc.sshd_e2e
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tests/boot/test_sshd_e2e.py"
            --build-dir "${CMAKE_BINARY_DIR}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(
        ipc.sshd_e2e
        PROPERTIES LABELS "ipc;net;ssh"
    )
    if(MICROKERNEL_BUILD_TZCODE)
        add_test(
            NAME libc.tzcode
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/tests/boot/test_tzcode.py"
                --sysroot "${SYSROOT_DIR}"
                --boot-tar "${CMAKE_BINARY_DIR}/boot.tar"
                --readelf llvm-readelf
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        )
        set_tests_properties(
            libc.tzcode
            PROPERTIES LABELS "libc"
        )
    endif()
endif()
