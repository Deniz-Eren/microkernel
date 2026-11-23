##
# @file     CMakeLists.txt
# @brief    Build the Ring 3 PCI virtio-net driver and TCP/IP server (/dev/net)
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(netserv main.c virtio_net.c net_proto.c)
