/**
 * @file     signal.c
 * @brief    SMP-safe Ring 3 signal registration, injection, and frame delivery
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <kernel/signal.h>
#include <kernel/ipc.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/uaccess.h>
#include <sys/errno.h>
#include <sys/signal.h>

/* need:: COMP_SIG_0001 COMP_PROC_0008 */

#define K_SIG_DFL 0ULL
#define K_SIG_IGN 1ULL
#define SIG_UNBLOCKABLE_MASK ((1u << SIGKILL) | (1u << SIGSTOP))

static void wake_signaled_thread(tcb_t *target) {
    uint32_t pend = __atomic_load_n(&target->pending_signals, __ATOMIC_ACQUIRE);
    if (!(pend & ~target->blocked_signals)) {
        return;
    }
    uint64_t sf = spin_lock_irqsave(&g_sched_lock);
    thread_state_t st = target->state;
    if (st == THREAD_WAIT_BLOCKED || st == THREAD_SLEEP_BLOCKED ||
        st == THREAD_FUTEX_BLOCKED) {
        target->futex_addr = 0;
        target->futex_has_timeout = 0;
        sched_ready_locked(target);
        spin_unlock_irqrestore(&g_sched_lock, sf);
        return;
    }
    spin_unlock_irqrestore(&g_sched_lock, sf);

    if (st >= THREAD_SEND_BLOCKED && st <= THREAD_RECEIVE_BLOCKED) {
        ipc_unblock_thread(target, -EINTR);
    }
}

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

    struct __k_sigaction act = {0};
    if (act_va) {
        if (copy_from_user(&act, act_va, sizeof(act)) < 0) {
            return -EFAULT;
        }
        if (act.handler != K_SIG_DFL && act.handler != K_SIG_IGN &&
            (act.handler < 0x400000ULL || act.handler >= 0x80000000ULL)) {
            return -EFAULT;
        }
    }

    struct __k_sigaction old = {0};
    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    old.handler = p->sig_handlers[sig];
    old.mask = p->sig_masks[sig];
    old.flags = p->sig_flags[sig];
    if (act_va) {
        p->sig_handlers[sig] = act.handler;
        p->sig_masks[sig] = act.mask & ~SIG_UNBLOCKABLE_MASK;
        p->sig_flags[sig] = act.flags;
        if (restorer) {
            p->sig_restorer = restorer;
        }
    }
    spin_unlock_irqrestore(&g_proc_lock, pf);

    if (oact_va && copy_to_user(oact_va, &old, sizeof(old)) < 0) {
        return -EFAULT;
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

long sys_sigpending(uint64_t set_va) {
    tcb_t *t = sched_current();
    if (!t) {
        return -EPERM;
    }
    if (!set_va) {
        return -EFAULT;
    }
    uint32_t pend = __atomic_load_n(&t->pending_signals, __ATOMIC_ACQUIRE);
    if (copy_to_user(set_va, &pend, sizeof(pend)) < 0) {
        return -EFAULT;
    }
    return 0;
}

void sys_sigreturn(trapframe_t *tf) {
    sig_frame_t f = {0};
    uint64_t frame_addr = arch_tf_arg0(tf);
    if (copy_from_user(&f, frame_addr, sizeof(f)) < 0) {
        printk("[sig] pid %u: bad signal frame %llx\n", proc_current()->pid,
               (unsigned long long)frame_addr);
        proc_exit_signal(SIGSEGV); /* noreturn */
    }
    tcb_t *t = sched_current();
    if (t) {
        t->blocked_signals = (uint32_t)f.saved_mask & ~SIG_UNBLOCKABLE_MASK;
    }
    memcpy(tf, &f, sizeof(trapframe_t));
    signal_deliver_check(tf);
}

static tcb_t *pick_target_thread_locked(pcb_t *p, uint64_t tid) {
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
    return target;
}

long sys_inject_sig(uint64_t pid, uint64_t tid, uint64_t sig) {
    if (sig >= NSIG) {
        return -EINVAL;
    }
    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    pcb_t *p = pid ? proc_find_locked((uint32_t)pid) : proc_current();
    if (!p || p->state != PROC_ALIVE) {
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return -ESRCH;
    }
    tcb_t *target = pick_target_thread_locked(p, tid);
    if (!target) {
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return -ESRCH;
    }
    if (sig == 0) {
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return 0;
    }
    __atomic_fetch_or(&target->pending_signals, (1u << sig), __ATOMIC_ACQ_REL);
    spin_unlock_irqrestore(&g_proc_lock, pf);

    wake_signaled_thread(target);
    return 0;
}

long sys_kill(int64_t pid, uint64_t sig) {
    if (sig >= NSIG) {
        return -EINVAL;
    }
    pcb_t *cur = proc_current();
    if (!cur) {
        return -EPERM;
    }
    tcb_t *targets[64];
    size_t ntargets = 0;

    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    if (pid > 0) {
        pcb_t *p = proc_find_locked((uint32_t)pid);
        if (!p || p->state != PROC_ALIVE) {
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return -ESRCH;
        }
        tcb_t *target = pick_target_thread_locked(p, 0);
        if (!target) {
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return -ESRCH;
        }
        if (sig > 0) {
            __atomic_fetch_or(&target->pending_signals, (1u << sig),
                              __ATOMIC_ACQ_REL);
            targets[ntargets++] = target;
        }
        spin_unlock_irqrestore(&g_proc_lock, pf);
        if (ntargets > 0) {
            wake_signaled_thread(targets[0]);
        }
        return 0;
    }

    int sent = 0;
    uint32_t target_pgid = (pid == 0) ? cur->pgid : (uint32_t)(-pid);
    for (pcb_t *p = proc_list_head(); p; p = p->next) {
        if (p->state != PROC_ALIVE) {
            continue;
        }
        if (pid == -1) {
            if (p->pid <= 1u || p == cur) {
                continue;
            }
        } else if (p->pgid != target_pgid) {
            continue;
        }
        tcb_t *target = pick_target_thread_locked(p, 0);
        if (target) {
            sent++;
            if (sig > 0) {
                __atomic_fetch_or(&target->pending_signals, (1u << sig),
                                  __ATOMIC_ACQ_REL);
                if (ntargets < 64u) {
                    targets[ntargets++] = target;
                }
            }
        }
    }
    spin_unlock_irqrestore(&g_proc_lock, pf);

    for (size_t i = 0; i < ntargets; i++) {
        wake_signaled_thread(targets[i]);
    }
    return sent > 0 ? 0 : -ESRCH;
}

/* Timer-IRQ-safe helper: post a signal to pid's main thread without calling
 * schedule() inside the timer interrupt handler. */
void signal_post_to_proc(uint32_t pid, int sig) {
    if (sig < 1 || sig >= NSIG) {
        return;
    }
    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    pcb_t *p = proc_find_locked(pid);
    if (!p || p->state != PROC_ALIVE) {
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return;
    }
    tcb_t *target = pick_target_thread_locked(p, 0);
    if (!target) {
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return;
    }
    __atomic_fetch_or(&target->pending_signals, (1u << (unsigned)sig),
                      __ATOMIC_ACQ_REL);
    spin_lock(&g_sched_lock);
    if (target->state == THREAD_WAIT_BLOCKED ||
        target->state == THREAD_SLEEP_BLOCKED ||
        target->state == THREAD_FUTEX_BLOCKED) {
        target->futex_addr = 0;
        target->futex_has_timeout = 0;
        sched_ready_locked(target);
    }
    spin_unlock(&g_sched_lock);
    spin_unlock_irqrestore(&g_proc_lock, pf);
}

void signal_deliver_check(trapframe_t *tf) {
    if (!arch_tf_is_user(tf)) {
        return; /* kernel frame: never inject here */
    }
    tcb_t *t = sched_current();
    if (!t || !t->proc) {
        return;
    }
    if (__atomic_load_n(&t->kill_pending, __ATOMIC_ACQUIRE)) {
        uint64_t sf = spin_lock_irqsave(&g_sched_lock);
        t->state = THREAD_ZOMBIE;
        schedule_with_lock(sf);
    }
    pcb_t *p = t->proc;

    for (;;) {
        uint32_t pend = __atomic_load_n(&t->pending_signals, __ATOMIC_ACQUIRE) &
                        ~t->blocked_signals;
        if (!pend) {
            return;
        }
        unsigned sig = __builtin_ctz(pend); /* lowest pending signal first */
        __atomic_fetch_and(&t->pending_signals, ~(1u << sig), __ATOMIC_ACQ_REL);

        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        uint64_t handler = p->sig_handlers[sig];
        uint32_t sig_mask = p->sig_masks[sig];
        uint32_t sig_flags = p->sig_flags[sig];
        uint64_t restorer = p->sig_restorer;
        if (handler != K_SIG_DFL && handler != K_SIG_IGN && restorer &&
            sig != SIGKILL && sig != SIGSTOP && (sig_flags & SA_RESETHAND)) {
            p->sig_handlers[sig] = K_SIG_DFL;
        }
        spin_unlock_irqrestore(&g_proc_lock, pf);

        if (handler == K_SIG_IGN && sig != SIGKILL && sig != SIGSTOP) {
            continue;
        }
        if (handler == K_SIG_DFL || !restorer || sig == SIGKILL ||
            sig == SIGSTOP) {
            printk("[sig] pid %u tid %u terminated by signal %u\n", p->pid,
                   t->tid, sig);
            proc_exit_signal((int)sig); /* noreturn */
        }

        sig_frame_t f;
        memcpy(&f, tf, sizeof(trapframe_t));
        f.sig = sig;
        f.saved_mask = t->blocked_signals;

        uint64_t frame_va =
            (arch_tf_user_sp(tf) - sizeof(sig_frame_t)) & ~15ULL;
        uint64_t ret_va = frame_va - 8; /* handler's return address slot */

        if (copy_to_user(frame_va, &f, sizeof(f)) < 0 ||
            copy_to_user(ret_va, &restorer, 8) < 0) {
            printk("[sig] pid %u: cannot push signal frame; terminating\n",
                   p->pid);
            proc_exit_signal((int)sig); /* noreturn */
        }

        uint32_t add_mask = sig_mask;
        if (!(sig_flags & SA_NODEFER)) {
            add_mask |= (1u << sig);
        }
        t->blocked_signals =
            (t->blocked_signals | add_mask) & ~SIG_UNBLOCKABLE_MASK;

        arch_tf_setup_signal_handler(tf, handler, sig, ret_va, t->iopl);
        return;
    }
}
