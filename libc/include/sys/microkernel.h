/**
 * @file     microkernel.h
 * @brief    System V AMD64 microkernel system call ABI and stubs
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stdint.h>

/* need:: ICD_PROC_0001 */
/* Microkernel system call ABI — shared between the kernel and libc. Register
 * convention: System V AMD64. RAX = number in / result out (negative errno on
 * failure). Args: RDI, RSI, RDX, R10, R8, R9. */

#define SYS_DEBUG_LOG 0
#define SYS_CHANNEL_CREATE 1
#define SYS_CHANNEL_DESTROY 2
#define SYS_CONNECT_ATTACH 3
#define SYS_CONNECT_DETACH 4
#define SYS_MSG_SENDV 5
#define SYS_MSG_RECEIVE 6
#define SYS_MSG_REPLYV 7
#define SYS_MSG_READV 8
#define SYS_MSG_WRITEV 9
#define SYS_MSG_SEND_PULSE 10
#define SYS_INTR_ATTACH_EVENT 11
#define SYS_INTR_UNMASK 12
#define SYS_PROC_CREATE 13
#define SYS_PROC_EXIT 14
#define SYS_VM_MAP 15
#define SYS_VM_UNMAP 16
#define SYS_VM_CLONE_COW 17
#define SYS_IPC_CLONE_CONNS 18
#define SYS_THREAD_CREATE 19
#define SYS_THREAD_GET_CTX 20
#define SYS_THREAD_RESET 21
#define SYS_THREAD_INJECT_SIG 22
#define SYS_SIGRETURN 23
#define SYS_SCHED_SET_PRIO 24
#define SYS_IOPL_SET 25    /* extension: grant Ring 3 port I/O (CAP_SYS_INTR) */
#define SYS_SIGACTION 26   /* extension: register Ring 3 signal handlers */
#define SYS_SIGPROCMASK 27 /* extension: read or update thread signal mask */
#define SYS_PROC_WAIT 28   /* extension: wait for child process exit status */
#define SYS_VM_PROTECT 29  /* extension: update user page permissions */
#define SYS_CLOCK_GETTIME 30
#define SYS_CLOCK_SETTIME 31
#define SYS_CLOCK_GETRES 32
#define SYS_CLOCK_NANOSLEEP 33
#define SYS_TIMER_CREATE 34
#define SYS_TIMER_DELETE 35
#define SYS_TIMER_SETTIME 36
#define SYS_TIMER_GETTIME 37
#define SYS_TIMER_GETOVERRUN 38
#define SYS_THREAD_SET_FSBASE 39
#define SYS_THREAD_EXIT 40
#define SYS_FUTEX 41
#define SYS_PROC_FORK 42
#define SYS_PROC_EXECVE 43
#define SYS_PROC_GETPID 44
#define SYS_PROC_GETPPID 45
#define SYS_FD_OP 46
#define SYS_SIGPENDING 47
#define SYS_GETENTROPY 48
#define SYS_SHM_OP 49
#define SYS_SHM_MAP 50
#define SYS_VM_MLOCK 51
#define SYS_SYS_STATS 52
#define SYS_COV_WRITE 53
#define SYS_SCHED_YIELD 54
#define SYS_PROC_TIMES 55
#define SYS_ALARM 56

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

#define COV_OP_REGISTER 1u
#define COV_OP_DUMP_ALL 2u

struct __k_cov_desc {
    uint64_t data_begin;
    uint64_t data_end;
    uint64_t cnts_begin;
    uint64_t cnts_end;
    uint64_t names_begin;
    uint64_t names_end;
    char name[32];
};

struct __k_tms {
    int64_t utime;
    int64_t stime;
    int64_t cutime;
    int64_t cstime;
};

#include <sys/arch/syscall.h>
