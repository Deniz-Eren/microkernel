/**
 * @file     fenv.h
 * @brief    POSIX.1-2024 floating-point environment control and exception flags
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _FENV_H
#define _FENV_H

/* need:: ICD_LIBC_0007 */

#include <stdint.h>

typedef uint32_t fexcept_t;

typedef struct {
    uint32_t __mxcsr;
    uint16_t __x87_cw;
    uint16_t __x87_sw;
} fenv_t;

/* Exception status flag bitmasks (aligned with x86-64 MXCSR/x87 status). */
#define FE_INVALID 0x0001
#define FE_DIVBYZERO 0x0004
#define FE_OVERFLOW 0x0008
#define FE_UNDERFLOW 0x0010
#define FE_INEXACT 0x0020
#define FE_ALL_EXCEPT                                                          \
    (FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW | FE_INEXACT)

/* Rounding direction modes (aligned with x86-64 MXCSR bits 14:13). */
#define FE_TONEAREST 0x0000
#define FE_DOWNWARD 0x2000
#define FE_UPWARD 0x4000
#define FE_TOWARDZERO 0x6000

/* Default floating-point environment sentinel pointer. */
#define FE_DFL_ENV ((const fenv_t *)-1)

int feclearexcept(int excepts);
int fegetexceptflag(fexcept_t *flagp, int excepts);
int feraiseexcept(int excepts);
int fesetexceptflag(const fexcept_t *flagp, int excepts);
int fetestexcept(int excepts);

int fegetround(void);
int fesetround(int round);

int fegetenv(fenv_t *envp);
int feholdexcept(fenv_t *envp);
int fesetenv(const fenv_t *envp);
int feupdateenv(const fenv_t *envp);

#endif /* _FENV_H */
