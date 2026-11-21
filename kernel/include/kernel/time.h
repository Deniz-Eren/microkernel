/**
 * @file     time.h
 * @brief    Kernel monotonic/realtime clocks, nanosleep, and POSIX timers
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stdint.h>

void time_tick(void);
uint64_t time_monotonic_ns(void);
uint64_t time_clock_now_ns(int clock_id);
void time_cleanup_proc(uint32_t pid);

long sys_clock_gettime(uint64_t clock_id, uint64_t tp_va);
long sys_clock_settime(uint64_t clock_id, uint64_t tp_va);
long sys_clock_getres(uint64_t clock_id, uint64_t res_va);
long sys_clock_nanosleep(uint64_t clock_id, uint64_t flags, uint64_t rqtp_va,
                         uint64_t rmtp_va);
long sys_timer_create(uint64_t clock_id, uint64_t sevp_va, uint64_t tid_va);
long sys_timer_delete(uint64_t timerid);
long sys_timer_settime(uint64_t timerid, uint64_t flags, uint64_t val_va,
                       uint64_t oval_va);
long sys_timer_gettime(uint64_t timerid, uint64_t val_va);
long sys_timer_getoverrun(uint64_t timerid);
