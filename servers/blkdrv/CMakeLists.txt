##
# @file     CMakeLists.txt
# @brief    Build the Ring 3 PCI virtio-blk block device driver (/dev/blk)
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(blkdrv main.c)
