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
add_library(c STATIC ${LIBC_SRCS})
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
        if(CMAKE_CURRENT_SOURCE_DIR MATCHES "/(servers|bin)(/|$)")
            target_compile_options(${name} PRIVATE ${COV_INSTR_FLAGS})
        else()
            target_compile_options(${name} PRIVATE -DMICROKERNEL_COVERAGE=1)
        endif()
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

add_subdirectory(servers/sysproc)
add_subdirectory(servers/pipeserv)
add_subdirectory(servers/vfsserv)
add_subdirectory(servers/ttyserv)
add_subdirectory(servers/sockserv)
add_subdirectory(servers/ipcserv)
add_subdirectory(servers/rtcdrv)
add_subdirectory(bin)
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
