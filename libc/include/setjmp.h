/**
 * @file     setjmp.h
 * @brief    POSIX.1-2024 non-local jump buffers and setjmp/sigsetjmp APIs
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SETJMP_H
#define _SETJMP_H

/* need:: ICD_LIBC_0006 */

typedef unsigned long jmp_buf[8];

typedef struct __sigjmp_buf_tag {
    jmp_buf __jb;
    int __savemask;
    unsigned int __sigmask;
} sigjmp_buf[1];

int setjmp(jmp_buf env) __attribute__((returns_twice));
_Noreturn void longjmp(jmp_buf env, int val);

int sigsetjmp(sigjmp_buf env, int savemask) __attribute__((returns_twice));
_Noreturn void siglongjmp(sigjmp_buf env, int val);

#endif /* _SETJMP_H */
