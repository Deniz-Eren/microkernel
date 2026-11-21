/**
 * @file     signal.h
 * @brief    Kernel signal frame layout and delivery interface
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

/* need:: ICD_SIG_0001 */
/* Ring 3 signal delivery (Milestone 4 / POSIX.1-2024).
 *
 * Delivery model: when a thread is about to return to user mode with a pending,
 * unblocked, handled signal, the kernel pushes a sig_frame_t (the interrupted
 * trapframe, signal number, and saved blocked_signals mask) onto the thread's
 * user stack and redirects execution to the registered handler with the
 * trampoline address as its return address. The handler returns into libc's
 * __sigreturn_trampoline, which invokes SYS_SIGRETURN(frame_va); the kernel
 * then copies the saved trapframe and blocked_signals mask back so the
 * interrupted context resumes transparently (including the interrupted
 * syscall's return value in RAX).
 *
 * Signals with SIG_IGN are discarded; signals with SIG_DFL terminate the
 * process. SIGKILL and SIGSTOP cannot be caught, ignored, or blocked.
 */
#include <stdint.h>
#include <kernel/trapframe.h>

/* Kernel/user shared frame: full trapframe + signal bookkeeping. */
typedef struct sig_frame {
    uint64_t gpr[15]; /* rax rbx rcx rdx rsi rdi rbp r8..r15 (tf order) */
    uint64_t vec, err;
    uint64_t rip, cs, rflags, rsp, ss;
    uint64_t sig;
    uint64_t saved_mask; /* saved blocked_signals (keeps 16-byte alignment) */
} sig_frame_t;

/* Check the current thread for deliverable signals and rewrite tf for handler
 * dispatch. Call at every return-to-user-mode boundary (the frame must be a
 * Ring 3 frame; non-user frames are ignored). May not return (default action
 * terminates the process). */
void signal_deliver_check(trapframe_t *tf);

/* Syscall implementations. */
long sys_sigaction(uint64_t sig, uint64_t act_va, uint64_t oact_va,
                   uint64_t restorer);
long sys_sigprocmask(uint64_t how, uint64_t set_va, uint64_t oset_va);
long sys_sigpending(uint64_t set_va);
void sys_sigreturn(trapframe_t *tf); /* never returns to its caller */
long sys_inject_sig(uint64_t pid, uint64_t tid, uint64_t sig);
void signal_post_to_proc(uint32_t pid, int sig);
