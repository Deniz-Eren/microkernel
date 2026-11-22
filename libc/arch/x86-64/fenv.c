/**
 * @file     fenv.c
 * @brief    x86-64 POSIX.1-2024 <fenv.h> MXCSR and x87 control implementation
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <fenv.h>
#include <stddef.h>

/* need:: COMP_LIBC_0007 */

#define MXCSR_ROUND_MASK 0x6000u
#define MXCSR_DEFAULT 0x1F80u
#define X87_CW_ROUND_MASK 0x0C00u
#define X87_CW_DEFAULT 0x037Fu

static inline uint32_t read_mxcsr(void) {
    uint32_t v = 0u;
    __asm__ volatile("vstmxcsr %0" : "=m"(v));
    return v;
}

static inline void write_mxcsr(uint32_t v) {
    __asm__ volatile("vldmxcsr %0" : : "m"(v) : "memory");
}

static inline uint16_t read_x87_cw(void) {
    uint16_t cw = 0u;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    return cw;
}

static inline void write_x87_cw(uint16_t cw) {
    __asm__ volatile("fldcw %0" : : "m"(cw) : "memory");
}

static inline uint16_t read_x87_sw(void) {
    uint16_t sw = 0u;
    __asm__ volatile("fnstsw %0" : "=m"(sw));
    return sw;
}

int feclearexcept(int excepts) {
    uint32_t mask = (uint32_t)excepts & (uint32_t)FE_ALL_EXCEPT;
    uint32_t mx = read_mxcsr();
    mx &= ~mask;
    write_mxcsr(mx);
    if (mask == (uint32_t)FE_ALL_EXCEPT) {
        __asm__ volatile("fnclex" : : : "memory");
    }
    return 0;
}

int fegetexceptflag(fexcept_t *flagp, int excepts) {
    if (!flagp) {
        return -1;
    }
    uint32_t mask = (uint32_t)excepts & (uint32_t)FE_ALL_EXCEPT;
    uint32_t active = (read_mxcsr() | (uint32_t)read_x87_sw()) & mask;
    *flagp = active;
    return 0;
}

int feraiseexcept(int excepts) {
    uint32_t mask = (uint32_t)excepts & (uint32_t)FE_ALL_EXCEPT;
    if (mask == 0u) {
        return 0;
    }
    uint32_t mx = read_mxcsr();
    mx |= mask;
    write_mxcsr(mx);
    return 0;
}

int fesetexceptflag(const fexcept_t *flagp, int excepts) {
    if (!flagp) {
        return -1;
    }
    uint32_t mask = (uint32_t)excepts & (uint32_t)FE_ALL_EXCEPT;
    uint32_t mx = read_mxcsr();
    mx = (mx & ~mask) | (*flagp & mask);
    write_mxcsr(mx);
    return 0;
}

int fetestexcept(int excepts) {
    uint32_t mask = (uint32_t)excepts & (uint32_t)FE_ALL_EXCEPT;
    return (int)((read_mxcsr() | (uint32_t)read_x87_sw()) & mask);
}

int fegetround(void) {
    return (int)(read_mxcsr() & MXCSR_ROUND_MASK);
}

int fesetround(int round) {
    if (round != FE_TONEAREST && round != FE_DOWNWARD && round != FE_UPWARD &&
        round != FE_TOWARDZERO) {
        return -1;
    }
    uint32_t mx = read_mxcsr();
    mx = (mx & ~MXCSR_ROUND_MASK) | (uint32_t)round;
    write_mxcsr(mx);

    /* Mirror rounding bits into x87 CW bits 11:10 (MXCSR bits 14:13 >> 3). */
    uint16_t cw = read_x87_cw();
    cw = (uint16_t)((cw & ~X87_CW_ROUND_MASK) |
                    (((uint32_t)round >> 3) & X87_CW_ROUND_MASK));
    write_x87_cw(cw);
    return 0;
}

int fegetenv(fenv_t *envp) {
    if (!envp) {
        return -1;
    }
    envp->__mxcsr = read_mxcsr();
    envp->__x87_cw = read_x87_cw();
    envp->__x87_sw = read_x87_sw();
    return 0;
}

int feholdexcept(fenv_t *envp) {
    if (!envp) {
        return -1;
    }
    if (fegetenv(envp) != 0) {
        return -1;
    }
    /* Mask all exceptions and clear sticky status flags (non-stop mode). */
    uint32_t mx = (envp->__mxcsr | 0x1F80u) & ~(uint32_t)FE_ALL_EXCEPT;
    write_mxcsr(mx);
    uint16_t cw = (uint16_t)(envp->__x87_cw | 0x003Fu);
    write_x87_cw(cw);
    __asm__ volatile("fnclex" : : : "memory");
    return 0;
}

int fesetenv(const fenv_t *envp) {
    if (!envp) {
        return -1;
    }
    if (envp == FE_DFL_ENV) {
        write_mxcsr(MXCSR_DEFAULT);
        write_x87_cw(X87_CW_DEFAULT);
        __asm__ volatile("fnclex" : : : "memory");
        return 0;
    }
    write_mxcsr(envp->__mxcsr);
    write_x87_cw(envp->__x87_cw);
    if ((envp->__x87_sw & (uint16_t)FE_ALL_EXCEPT) == 0u) {
        __asm__ volatile("fnclex" : : : "memory");
    }
    return 0;
}

int feupdateenv(const fenv_t *envp) {
    if (!envp) {
        return -1;
    }
    int saved_except = fetestexcept(FE_ALL_EXCEPT);
    if (fesetenv(envp) != 0) {
        return -1;
    }
    return feraiseexcept(saved_except);
}
