/**
 * @file     wait.h
 * @brief    POSIX.1-2024 process termination status macros and wait functions
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

/* need:: ICD_PROC_0004 */

#include <signal.h>
#include <sys/types.h>

#define WNOHANG 1
#define WUNTRACED 2
#define WSTOPPED 2
#define WEXITED 4
#define WCONTINUED 8
#define WNOWAIT 0x1000000

#define WIFEXITED(s) (((s)&0x7F) == 0)
#define WEXITSTATUS(s) (((s) >> 8) & 0xFF)
#define WIFSIGNALED(s)                                                         \
    ((((s)&0x7F) != 0) && (((s)&0xFF) != 0x7F) && ((s) != 0xFFFF))
#define WTERMSIG(s) ((s)&0x7F)
#define WIFSTOPPED(s) (((s)&0xFF) == 0x7F)
#define WSTOPSIG(s) (((s) >> 8) & 0xFF)
#define WIFCONTINUED(s) ((s) == 0xFFFF)

typedef enum { P_ALL, P_PID, P_PGID } idtype_t;

pid_t wait(int *stat_loc);
pid_t waitpid(pid_t pid, int *stat_loc, int options);
int waitid(idtype_t idtype, id_t id, siginfo_t *restrict infop, int options);

#endif /* _SYS_WAIT_H */
