/**
 * @file     assert.h
 * @brief    ISO C17 and POSIX.1-2024 runtime and compile-time assertions
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

/* need:: ICD_LIBC_0005 */

#ifndef _MICROKERNEL_ASSERT_DECL
#define _MICROKERNEL_ASSERT_DECL
_Noreturn void __assert_fail(const char *expr, const char *file,
                             unsigned int line, const char *func);
#endif /* _MICROKERNEL_ASSERT_DECL */

#undef assert

#ifdef NDEBUG
#define assert(ignore) ((void)0)
#else
#define assert(expr)                                                           \
    ((expr)                                                                    \
         ? (void)0                                                             \
         : __assert_fail(#expr, __FILE__, (unsigned int)__LINE__, __func__))
#endif

#ifndef static_assert
#define static_assert _Static_assert
#endif
