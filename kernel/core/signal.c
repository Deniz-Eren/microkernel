/**
 * @file     signal.c
 * @brief    Ring 3 signal registration, injection, masks, and frame delivery
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <kernel/signal.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <kernel/uaccess.h>
#include <kernel/string.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <kernel/ipc.h>

/* need:: COMP_SIG_0001 */

#define K_SIG_DFL 0ULL
#define K_SIG_IGN 1ULL
#define SIG_UNBLOCKABLE_MASK ((1u << SIGKILL) | (1u << SIGSTOP))

long sys_sigaction(uint64_t sig, uint64_t act_va, uint64_t oact_va,
                   uint64_t restorer) {
    pcb_t *p = proc_current();
    if (!p) {
        return -EPERM;
    }
    if (sig < 1 || sig >= NSIG) {
        return -EINVAL;
    }
    if (act_va && (sig == SIGKILL || sig == SIGSTOP)) {
        return -EINVAL;
    }

    if (oact_va) {
        struct __k_sigaction old = {
            .handler = p->sig_handlers[sig],
            .mask = p->sig_masks[sig],
            .flags = p->sig_flags[sig],
        };
        if (copy_to_user(oact_va, &old, sizeof(old)) < 0) {
            return -EFAULT;
        }
    }

    if (act_va) {
        struct __k_sigaction act = {0};
        if (copy_from_user(&act, act_va, sizeof(act)) < 0) {
            return -EFAULT;
        }
        if (act.handler != K_SIG_DFL && act.handler != K_SIG_IGN &&
            (act.handler < 0x400000ULL || act.handler >= 0x80000000ULL)) {
            return -EFAULT;
        }
        p->sig_handlers[sig] = act.handler;
        p->sig_masks[sig] = act.mask & ~SIG_UNBLOCKABLE_MASK;
        p->sig_flags[sig] = act.flags;
        if (restorer) {
            p->sig_restorer = restorer;
        }
    }
    return 0;
}

long sys_sigprocmask(uint64_t how, uint64_t set_va, uint64_t oset_va) {
    tcb_t *t = sched_current();
    if (!t) {
        return -EPERM;
    }
    if (oset_va) {
        uint32_t old_mask = t->blocked_signals;
        if (copy_to_user(oset_va, &old_mask, sizeof(old_mask)) < 0) {
            return -EFAULT;
        }
    }
    if (set_va) {
        uint32_t new_mask = 0;
        if (copy_from_user(&new_mask, set_va, sizeof(new_mask)) < 0) {
            return -EFAULT;
        }
        new_mask &= ~SIG_UNBLOCKABLE_MASK;
        switch (how) {
        case SIG_BLOCK:
            t->blocked_signals |= new_mask;
            break;
        case SIG_UNBLOCK:
            t->blocked_signals &= ~new_mask;
            break;
        case SIG_SETMASK:
            t->blocked_signals = new_mask;
            break;
        default:
            return -EINVAL;
        }
    }
    return 0;
}

void sys_sigreturn(trapframe_t *tf) {
    sig_frame_t f = {0};
    if (copy_from_user(&f, tf->rdi, sizeof(f)) < 0) {
        printk("[sig] pid %u: bad signal frame %llx\n", proc_current()->pid,
               (unsigned long long)tf->rdi);
        proc_exit_signal(SIGSEGV); /* noreturn */
    }
    tcb_t *t = sched_current();
    if (t) {
        t->blocked_signals = (uint32_t)f.saved_mask & ~SIG_UNBLOCKABLE_MASK;
    }
    /* Restore the interrupted context wholesale. syscall_handler returns
     * without touching RAX afterwards, so the interrupted syscall's return
     * value (saved in the frame) reaches user mode intact. */
    memcpy(tf, &f, sizeof(trapframe_t));
    signal_deliver_check(tf);
}

long sys_inject_sig(uint64_t pid, uint64_t tid, uint64_t sig) {
    if (sig >= NSIG) {
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
        if (tid ? t->tid == (uint32_t)tid : (!target || t->tid < target->tid)) {
            target = t;
            if (tid) {
                break;
            }
        }
    }
    if (!target) {
        return -ESRCH;
    }
    if (sig == 0) {
        return 0; /* POSIX null signal: error check only */
    }

    target->pending_signals |= (1u << sig);

    /* A thread blocked in IPC or waitpid wakes with -EINTR (kill semantics);
     * the signal itself is delivered at its next return to user mode. */
    if (target->state >= THREAD_SEND_BLOCKED &&
        target->state <= THREAD_RECEIVE_BLOCKED) {
        ipc_unblock_thread(target, -EINTR);
    } else if (target->state == THREAD_WAIT_BLOCKED) {
        sched_unblock(target);
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
    pcb_t *p = t->proc;

    for (;;) {
        uint32_t pend = t->pending_signals & ~t->blocked_signals;
        if (!pend) {
            return;
        }
        unsigned sig = __builtin_ctz(pend); /* lowest pending signal first */
        t->pending_signals &= ~(1u << sig);

        uint64_t handler = p->sig_handlers[sig];
        if (handler == K_SIG_IGN && sig != SIGKILL && sig != SIGSTOP) {
            continue; /* ignored signal: discard and check remaining */
        }
        if (handler == K_SIG_DFL || !p->sig_restorer || sig == SIGKILL ||
            sig == SIGSTOP) {
            /* Default action: terminate the process. */
            printk("[sig] pid %u tid %u terminated by signal %u\n", p->pid,
                   t->tid, sig);
            proc_exit_signal((int)sig); /* noreturn */
        }

        sig_frame_t f;
        memcpy(&f, tf, sizeof(trapframe_t));
        f.sig = sig;
        f.saved_mask = t->blocked_signals;

        uint64_t frame_va = (tf->rsp - sizeof(sig_frame_t)) & ~15ULL;
        uint64_t ret_va = frame_va - 8; /* handler's return address slot */

        if (copy_to_user(frame_va, &f, sizeof(f)) < 0 ||
            copy_to_user(ret_va, &p->sig_restorer, 8) < 0) {
            printk("[sig] pid %u: cannot push signal frame; terminating\n",
                   p->pid);
            proc_exit_signal((int)sig); /* noreturn */
        }

        uint32_t add_mask = p->sig_masks[sig];
        if (!(p->sig_flags[sig] & SA_NODEFER)) {
            add_mask |= (1u << sig);
        }
        t->blocked_signals =
            (t->blocked_signals | add_mask) & ~SIG_UNBLOCKABLE_MASK;
        if (p->sig_flags[sig] & SA_RESETHAND) {
            p->sig_handlers[sig] = K_SIG_DFL;
        }

        tf->rsp = ret_va;
        tf->rip = handler;
        tf->rdi = sig; /* handler(int sig) */
        tf->rax = 0;
        tf->rflags = 0x202ULL | (((uint64_t)t->iopl & 3ULL) << 12);
        return;
    }
}
