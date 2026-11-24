##
# @file     CMakeLists.txt
# @brief    Build configuration for the Ring 3 dynamic linker (ld.so) test
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

# Static driver: copies the LD_LIBRARY_PATH library variant into /tmp/libs and
# execs the dynamic executable with and without the search path set.
microkernel_add_user_executable(dlsoctest test_dlsoctest.c)

# Shared libraries under test: the /boot default variant (value 1) and the
# LD_LIBRARY_PATH variant (value 2, copied to /tmp/libs/libmtest.so).
microkernel_add_user_shared_library(libmtest BOOT_NAME libmtest.so
                                    test_libmtest.c)
microkernel_add_user_shared_library(libmtest_alt BOOT_NAME libmtest_alt.so
                                    test_libmtest_alt.c)

# Dynamic executable under test: freestanding ET_DYN PIE with
# PT_INTERP=/boot/ld.so and DT_NEEDED=libmtest.so.
set(dlso_dyn_obj_target dlsoctest_dyn_obj)
add_library(${dlso_dyn_obj_target} OBJECT test_dlsoctest_dyn.c)
set(dlso_pic_cflags ${USER_CFLAGS})
list(REMOVE_ITEM dlso_pic_cflags -fno-pic -fno-pie)
list(APPEND dlso_pic_cflags -fPIC)
target_compile_options(${dlso_dyn_obj_target} PRIVATE ${dlso_pic_cflags})
target_include_directories(${dlso_dyn_obj_target} SYSTEM PRIVATE
    ${SYSROOT_DIR}/usr/include
    ${CLANG_RESOURCE_DIR}/include
)
add_dependencies(${dlso_dyn_obj_target} install_headers)

set(dlso_dyn_start_o ${CMAKE_CURRENT_BINARY_DIR}/dlsoctest_dyn_start.o)
add_custom_command(
    OUTPUT ${dlso_dyn_start_o}
    COMMAND ${CMAKE_C_COMPILER} ${USER_CFLAGS}
            -target ${MICROKERNEL_TARGET_TRIPLE}
            -c ${CMAKE_CURRENT_SOURCE_DIR}/dyn_start.S
            -o ${dlso_dyn_start_o}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/dyn_start.S install_headers
    COMMENT "Stage 3: compiling dlsoctest_dyn entry (dyn_start.S)"
    VERBATIM
)

set(dlso_dyn_out ${CMAKE_CURRENT_BINARY_DIR}/dlsoctest_dyn)
add_custom_command(
    OUTPUT ${dlso_dyn_out}
    COMMAND ${CMAKE_LINKER} -pie -e _start
            --dynamic-linker=/boot/ld.so
            --no-undefined
            --build-id=none
            ${dlso_dyn_start_o}
            $<TARGET_OBJECTS:${dlso_dyn_obj_target}>
            ${CMAKE_CURRENT_BINARY_DIR}/libmtest.so
            -o ${dlso_dyn_out}
    DEPENDS ${dlso_dyn_start_o} ${dlso_dyn_obj_target} libmtest
    COMMENT "Stage 3: linking dynamic linker test executable dlsoctest_dyn"
    VERBATIM
)
add_custom_target(dlsoctest_dyn_build ALL DEPENDS ${dlso_dyn_out})
set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_MODULES
    "dlsoctest_dyn=${dlso_dyn_out}")
set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_DEPS dlsoctest_dyn_build)
