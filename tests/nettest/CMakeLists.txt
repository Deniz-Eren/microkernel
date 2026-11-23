##
# @file     CMakeLists.txt
# @brief    Build the Ring 3 virtio-net and TCP/IP/loopback test suite
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(nettest test_net.c)
