##
# @file     CMakeLists.txt
# @brief    Build the Ring 3 MC146818 CMOS RTC user-space device driver
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(rtcdrv
    arch/${MICROKERNEL_ARCH}/cmos_rtc.c
)
