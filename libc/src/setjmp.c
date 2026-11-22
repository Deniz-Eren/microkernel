/**
 * @file     setjmp.c
 * @brief    POSIX.1-2024 siglongjmp() wrapper and sigjmp_buf layout assertions
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <setjmp.h>
#include <signal.h>
#include <stddef.h>

/* need:: COMP_LIBC_0006 */

_Static_assert(offsetof(struct __sigjmp_buf_tag, __savemask) == 64,
               "setjmp.S __savemask offset mismatch");
_Static_assert(offsetof(struct __sigjmp_buf_tag, __sigmask) == 68,
               "setjmp.S __sigmask offset mismatch");
_Static_assert(SIG_SETMASK == 2, "setjmp.S SIG_SETMASK constant mismatch");

_Noreturn void siglongjmp(sigjmp_buf env, int val) {
    if (env[0].__savemask) {
        sigset_t mask = (sigset_t)env[0].__sigmask;
        (void)sigprocmask(SIG_SETMASK, &mask, 0);
    }
    longjmp(env[0].__jb, val);
}
