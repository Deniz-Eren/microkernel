// Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
// SPDX-License-Identifier: MIT

#include <kernel/signal.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <kernel/uaccess.h>
#include <kernel/string.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <kernel/ipc.h>

/* need:: COMP_SIG_0001 */

long sys_sigaction(uint64_t sig, uint64_t handler, uint64_t restorer) {
    pcb_t *p = proc_current();
    if (!p) {
        return -EPERM;
    }
    if (sig < 1 || sig >= NSIG) {
        return -EINVAL;
    }
    if (handler &&
        (handler < 0x400000ULL || handler >= 0x80000000ULL)) {
        return -EFAULT; /* handler must be user code (0 = default action) */
    }
    p->sig_handlers[sig] = handler;
    if (restorer) {
        p->sig_restorer = restorer;
    }
    return 0;
}

void sys_sigreturn(trapframe_t *tf) {
    sig_frame_t f;
    if (copy_from_user(&f, tf->rdi, sizeof(f)) < 0) {
        printk("[sig] pid %u: bad signal frame %llx\n", proc_current()->pid,
               (unsigned long long)tf->rdi);
        proc_exit_current(128 + SIGSEGV); /* noreturn */
    }
    /* Restore the interrupted context wholesale. syscall_handler returns
     * without touching RAX afterwards, so the interrupted syscall's return
     * value (saved in the frame) reaches user mode intact. */
    memcpy(tf, &f, sizeof(trapframe_t));
}

long sys_inject_sig(uint64_t pid, uint64_t tid, uint64_t sig) {
    if (sig < 1 || sig >= NSIG) {
        return -EINVAL;
    }
    pcb_t *p = pid ? proc_find((uint32_t)pid) : proc_current();
    if (!p) {
        return -ESRCH;
    }
    /* tid 0 = the process's initial (main) thread: the pcb thread list is
     * prepend-ordered, so pick the lowest live tid (creation order). */
    tcb_t *target = NULL;
    for (tcb_t *t = p->threads; t; t = t->proc_next) {
        if (t->state == THREAD_ZOMBIE) {
            continue;
        }
        if (tid ? t->tid == (uint32_t)tid
                : (!target || t->tid < target->tid)) {
            target = t;
            if (tid) {
                break;
            }
        }
    }
    if (!target) {
        return -ESRCH;
    }

    target->pending_signals |= (1u << sig);

    /* A thread blocked in IPC wakes with -EINTR (kill semantics);
     * the signal itself is delivered at its next return to user mode. */
    if (target->state >= THREAD_SEND_BLOCKED &&
        target->state <= THREAD_RECEIVE_BLOCKED) {
        ipc_unblock_thread(target, -EINTR);
    }
    return 0;
}

void signal_deliver_check(trapframe_t *tf) {
    if (!(tf->cs & 3)) {
        return; /* kernel frame: never inject here */
    }
    tcb_t *t = sched_current();
    if (!t || !t->proc) {
        return;
    }
    uint32_t pend = t->pending_signals & ~t->blocked_signals;
    if (!pend) {
        return;
    }
    unsigned sig = __builtin_ctz(pend); /* lowest pending signal first */
    t->pending_signals &= ~(1u << sig);

    pcb_t *p = t->proc;
    uint64_t handler = p->sig_handlers[sig];
    if (!handler || !p->sig_restorer) {
        /* Default action: terminate the process. */
        printk("[sig] pid %u tid %u terminated by signal %u\n", p->pid, t->tid,
               sig);
        proc_exit_current(128 + (int)sig); /* noreturn */
    }

    sig_frame_t f;
    memcpy(&f, tf, sizeof(trapframe_t));
    f.sig = sig;
    f.pad = 0;

    uint64_t frame_va = (tf->rsp - sizeof(sig_frame_t)) & ~15ULL;
    uint64_t ret_va = frame_va - 8; /* handler's return address slot */

    if (copy_to_user(frame_va, &f, sizeof(f)) < 0 ||
        copy_to_user(ret_va, &p->sig_restorer, 8) < 0) {
        printk("[sig] pid %u: cannot push signal frame; terminating\n",
               p->pid);
        proc_exit_current(128 + (int)sig); /* noreturn */
    }

    tf->rsp = ret_va;
    tf->rip = handler;
    tf->rdi = sig; /* handler(int sig) */
    tf->rax = 0;
    tf->rflags = 0x202; /* clean flags, IF=1 */
}
