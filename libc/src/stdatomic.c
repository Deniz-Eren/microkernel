/**
 * @file     stdatomic.c
 * @brief    Out-of-line C17 <stdatomic.h> fence and atomic_flag definitions
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdatomic.h>

/* need:: COMP_LIBC_0003 */

#undef atomic_thread_fence
void atomic_thread_fence(memory_order order) {
    __c11_atomic_thread_fence(order);
}

#undef atomic_signal_fence
void atomic_signal_fence(memory_order order) {
    __c11_atomic_signal_fence(order);
}

#undef atomic_flag_test_and_set
_Bool atomic_flag_test_and_set(volatile atomic_flag *object) {
    return __c11_atomic_exchange(&object->_Value, 1, __ATOMIC_SEQ_CST);
}

#undef atomic_flag_test_and_set_explicit
_Bool atomic_flag_test_and_set_explicit(volatile atomic_flag *object,
                                        memory_order order) {
    return __c11_atomic_exchange(&object->_Value, 1, order);
}

#undef atomic_flag_clear
void atomic_flag_clear(volatile atomic_flag *object) {
    __c11_atomic_store(&object->_Value, 0, __ATOMIC_SEQ_CST);
}

#undef atomic_flag_clear_explicit
void atomic_flag_clear_explicit(volatile atomic_flag *object,
                                memory_order order) {
    __c11_atomic_store(&object->_Value, 0, order);
}
