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
#include <kernel/arch.h>
#include <kernel/ipc.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <kernel/smp.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/time.h>
#include <kernel/uaccess.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <time.h>

/* need:: COMP_SIG_0001 COMP_PROC_0008 */

#define K_SIG_DFL 0ULL
#define K_SIG_IGN 1ULL
#define SIG_UNBLOCKABLE_MASK ((1u << SIGKILL) | (1u << SIGSTOP))

static int signal_is_default_ignore(unsigned sig) {
    return sig == SIGCHLD || sig == SIGURG || sig == SIGCONT;
}

static int signal_is_ignored_locked(pcb_t *p, unsigned sig) {
    if (sig == SIGKILL || sig == SIGSTOP) {
        return 0;
    }
    uint64_t handler = p->sig_handlers[sig];
    if (handler == K_SIG_IGN) {
        return 1;
    }
    return handler == K_SIG_DFL && signal_is_default_ignore(sig);
}

static void signal_store_value_locked(pcb_t *p, unsigned sig, uint64_t value,
                                      int code, uint32_t sender_pid,
                                      uint32_t sender_uid) {
    if (sig == 0u || sig >= 32u) {
        return;
    }
    p->sig_values[sig] = value;
    p->sig_codes[sig] = (uint8_t)code;
    p->sig_senders[sig] = sender_pid;
    p->sig_sender_uids[sig] = sender_uid;
}

static int proc_stop_locked(pcb_t *p, tcb_t *cur, int sig) {
    int need_ipi = 0;
    p->stopped = 1u;
    p->stop_sig = (uint8_t)sig;
    pcb_t *parent = p->parent_pid ? proc_find_locked(p->parent_pid) : NULL;
    for (tcb_t *t = p->threads; t; t = t->proc_next) {
        if (t == cur) {
            t->state = THREAD_STOPPED;
        } else if (t->state == THREAD_READY) {
            sched_remove_ready_locked(t);
            t->state = THREAD_STOPPED;
        } else if (t->state == THREAD_RUNNING) {
            t->state = THREAD_STOPPED;
            if (__atomic_load_n(&t->on_cpu, __ATOMIC_ACQUIRE) != 0u) {
                need_ipi = 1;
            }
        } else if (t->state == THREAD_SLEEP_BLOCKED) {
            t->state = THREAD_STOPPED;
        }
    }
    if (parent) {
        for (tcb_t *wt = parent->threads; wt; wt = wt->proc_next) {
            if (wt->state == THREAD_WAIT_BLOCKED && !wt->in_sigsuspend) {
                sched_ready_locked(wt);
            }
        }
    }
    return need_ipi;
}

static void proc_resume_locked(pcb_t *p, int mark_cont) {
    if (!p->stopped) {
        return;
    }
    p->stopped = 0u;
    p->stop_sig = 0u;
    if (mark_cont) {
        p->cont_pending = 1u;
    }
    pcb_t *parent = p->parent_pid ? proc_find_locked(p->parent_pid) : NULL;
    spin_lock(&g_sched_lock);
    for (tcb_t *t = p->threads; t; t = t->proc_next) {
        if (t->state == THREAD_STOPPED) {
            sched_ready_locked(t);
        }
    }
    if (parent && mark_cont) {
        for (tcb_t *wt = parent->threads; wt; wt = wt->proc_next) {
            if (wt->state == THREAD_WAIT_BLOCKED && !wt->in_sigsuspend) {
                sched_ready_locked(wt);
            }
        }
    }
    spin_unlock(&g_sched_lock);
}

static void signal_post_locked(pcb_t *p, tcb_t *target, unsigned sig,
                               uint64_t value, int code, uint32_t sender_pid,
                               uint32_t sender_uid) {
    signal_store_value_locked(p, sig, value, code, sender_pid, sender_uid);
    __atomic_fetch_or(&target->pending_signals, (1u << sig), __ATOMIC_ACQ_REL);
    if (p->stopped) {
        if (sig == SIGCONT) {
            proc_resume_locked(p, 1);
        } else if (sig == SIGKILL) {
            proc_resume_locked(p, 0);
        }
    }
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

static void wake_signaled_thread(tcb_t *target) {
    if (!target) {
        return;
    }
    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    pcb_t *p = target->proc;
    if (!p || p->state != PROC_ALIVE || p->stopped) {
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return;
    }
    uint32_t pend = __atomic_load_n(&target->pending_signals, __ATOMIC_ACQUIRE);
    uint32_t del = pend & ~target->blocked_signals;
    int deliverable = 0;
    while (del) {
        unsigned s = (unsigned)__builtin_ctz(del);
        if (!signal_is_ignored_locked(p, s)) {
            deliverable = 1;
            break;
        }
        del &= ~(1u << s);
    }
    spin_lock(&g_sched_lock);
    int in_waitset =
        target->sig_wait_set != 0u && (pend & target->sig_wait_set) != 0u;
    if (!deliverable && !in_waitset) {
        spin_unlock(&g_sched_lock);
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return;
    }
    thread_state_t st = target->state;
    if (st == THREAD_WAIT_BLOCKED || st == THREAD_SLEEP_BLOCKED ||
        st == THREAD_FUTEX_BLOCKED) {
        if (st == THREAD_WAIT_BLOCKED && target->in_sigsuspend &&
            !deliverable) {
            spin_unlock(&g_sched_lock);
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return;
        }
        target->futex_addr = 0;
        target->futex_is_shared = 0u;
        target->futex_has_timeout = 0;
        target->sig_wait_set = 0;
        sched_ready_locked(target);
        spin_unlock(&g_sched_lock);
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return;
    }
    spin_unlock(&g_sched_lock);
    spin_unlock_irqrestore(&g_proc_lock, pf);

    if (st >= THREAD_SEND_BLOCKED && st <= THREAD_RECEIVE_BLOCKED) {
        ipc_unblock_thread(target, -EINTR);
    }
}

void signal_notify_child_locked(pcb_t *parent, uint32_t child_pid,
                                uint32_t child_uid) {
    if (!parent || parent->state != PROC_ALIVE) {
        return;
    }
    uint64_t handler = parent->sig_handlers[SIGCHLD];
    int has_handler = (handler != K_SIG_DFL && handler != K_SIG_IGN);
    tcb_t *waiter = NULL;
    tcb_t *blocked_t = NULL;
    for (tcb_t *t = parent->threads; t; t = t->proc_next) {
        if (t->state == THREAD_ZOMBIE) {
            continue;
        }
        if (t->sig_wait_set & (1u << SIGCHLD)) {
            waiter = t;
            break;
        }
        if (!blocked_t && (t->blocked_signals & (1u << SIGCHLD))) {
            blocked_t = t;
        }
    }
    if (!has_handler && !waiter && !blocked_t) {
        return;
    }
    tcb_t *target =
        waiter ? waiter
               : (blocked_t ? blocked_t : pick_target_thread_locked(parent, 0));
    if (!target) {
        return;
    }
    signal_store_value_locked(parent, SIGCHLD, 0, K_SI_USER, child_pid,
                              child_uid);
    __atomic_fetch_or(&target->pending_signals, (1u << SIGCHLD),
                      __ATOMIC_ACQ_REL);
    spin_lock(&g_sched_lock);
    if ((target->state == THREAD_FUTEX_BLOCKED &&
         (target->sig_wait_set & (1u << SIGCHLD))) ||
        (has_handler && (target->state == THREAD_WAIT_BLOCKED ||
                         target->state == THREAD_SLEEP_BLOCKED ||
                         target->state == THREAD_FUTEX_BLOCKED))) {
        target->futex_addr = 0;
        target->futex_is_shared = 0u;
        target->futex_has_timeout = 0;
        target->sig_wait_set = 0;
        sched_ready_locked(target);
    }
    spin_unlock(&g_sched_lock);
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

long sys_sigtimedwait(uint64_t set_va, uint64_t info_va, uint64_t timeout_va,
                      uint64_t clock_id) {
    (void)clock_id; /* POSIX sigtimedwait timeout is a relative duration. */
    tcb_t *t = sched_current();
    pcb_t *p = t ? t->proc : NULL;
    if (!t || !p) {
        return -EPERM;
    }
    if (!set_va) {
        return -EINVAL;
    }
    uint32_t set = 0;
    if (copy_from_user(&set, set_va, sizeof(set)) < 0) {
        return -EFAULT;
    }
    set &= ~SIG_UNBLOCKABLE_MASK; /* SIGKILL/SIGSTOP are never waitable */

    uint64_t rel_ns = 0;
    uint64_t deadline = 0;
    uint8_t has_timeout = 0u;
    if (timeout_va) {
        struct timespec ts = {0};
        if (copy_from_user(&ts, timeout_va, sizeof(ts)) < 0) {
            return -EFAULT;
        }
        if (ts.tv_sec < 0 || ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000L) {
            return -EINVAL;
        }
        rel_ns = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
        deadline = time_clock_now_ns(CLOCK_MONOTONIC) + rel_ns;
        has_timeout = 1u;
    }

    for (;;) {
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        spin_lock(&g_sched_lock);
        uint32_t pend =
            __atomic_load_n(&t->pending_signals, __ATOMIC_ACQUIRE) & set;
        if (pend) {
            unsigned sig = (unsigned)__builtin_ctz(pend);
            __atomic_fetch_and(&t->pending_signals, ~(1u << sig),
                               __ATOMIC_ACQ_REL);
            struct __k_siginfo ki = {0};
            ki.signo = (int32_t)sig;
            ki.code = p->sig_codes[sig];
            ki.pid = p->sig_senders[sig];
            ki.uid = p->sig_sender_uids[sig];
            ki.value = p->sig_values[sig];
            p->sig_values[sig] = 0;
            p->sig_codes[sig] = 0;
            p->sig_senders[sig] = 0;
            p->sig_sender_uids[sig] = 0;
            t->sig_wait_set = 0;
            spin_unlock(&g_sched_lock);
            spin_unlock_irqrestore(&g_proc_lock, pf);
            if (info_va && copy_to_user(info_va, &ki, sizeof(ki)) < 0) {
                return -EFAULT;
            }
            return (long)sig;
        }
        if (has_timeout && (rel_ns == 0ULL ||
                            time_clock_now_ns(CLOCK_MONOTONIC) >= deadline)) {
            t->sig_wait_set = 0;
            spin_unlock(&g_sched_lock);
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return -EAGAIN;
        }
        uint32_t del = __atomic_load_n(&t->pending_signals, __ATOMIC_ACQUIRE) &
                       ~t->blocked_signals;
        int would_deliver = 0;
        while (del) {
            unsigned s = (unsigned)__builtin_ctz(del);
            if (!signal_is_ignored_locked(p, s)) {
                would_deliver = 1;
                break;
            }
            del &= ~(1u << s);
        }
        if (would_deliver) {
            t->sig_wait_set = 0;
            spin_unlock(&g_sched_lock);
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return -EINTR;
        }
        t->sig_wait_set = set;
        t->futex_has_timeout = has_timeout;
        t->sleep_clock_id = (uint8_t)CLOCK_MONOTONIC;
        t->sleep_deadline_ns = deadline;
        t->futex_addr = 0;
        t->futex_is_shared = 0u;
        t->futex_woken = 0;
        t->state = THREAD_FUTEX_BLOCKED;
        spin_unlock(&g_proc_lock);
        schedule_with_lock(pf);
    }
}

long sys_sigsuspend(uint64_t mask_va) {
    tcb_t *t = sched_current();
    pcb_t *p = t ? t->proc : NULL;
    if (!t || !p) {
        return -EPERM;
    }
    if (!mask_va) {
        return -EFAULT;
    }
    uint32_t mask = 0;
    if (copy_from_user(&mask, mask_va, sizeof(mask)) < 0) {
        return -EFAULT;
    }
    mask &= ~SIG_UNBLOCKABLE_MASK;
    t->saved_sigmask = t->blocked_signals;
    t->has_saved_sigmask = 1u;
    t->blocked_signals = mask;

    for (;;) {
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        spin_lock(&g_sched_lock);
        uint32_t del = __atomic_load_n(&t->pending_signals, __ATOMIC_ACQUIRE) &
                       ~t->blocked_signals;
        int any_deliverable = 0;
        while (del) {
            unsigned s = (unsigned)__builtin_ctz(del);
            if (!signal_is_ignored_locked(p, s)) {
                any_deliverable = 1;
                break;
            }
            del &= ~(1u << s);
        }
        if (any_deliverable) {
            spin_unlock(&g_sched_lock);
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return -EINTR;
        }
        t->in_sigsuspend = 1u;
        t->state = THREAD_WAIT_BLOCKED;
        spin_unlock(&g_proc_lock);
        schedule_with_lock(pf);
        t->in_sigsuspend = 0u;
    }
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

long sys_inject_sig(uint64_t pid, uint64_t tid, uint64_t sig) {
    if (sig >= NSIG) {
        return -EINVAL;
    }
    pcb_t *cur = proc_current();
    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    pcb_t *p = pid ? proc_find_locked((uint32_t)pid) : cur;
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
    signal_post_locked(p, target, (unsigned)sig, 0, K_SI_USER,
                       cur ? cur->pid : 0u, cur ? cur->uid : 0u);
    spin_unlock_irqrestore(&g_proc_lock, pf);

    wake_signaled_thread(target);
    return 0;
}

long sys_sigqueue(int64_t pid, uint64_t value_va, uint64_t sig) {
    if (sig >= NSIG) {
        return -EINVAL;
    }
    if (pid <= 0) {
        return -ESRCH;
    }
    pcb_t *cur = proc_current();
    if (!cur) {
        return -EPERM;
    }
    union {
        uint64_t u64;
        int sival_int;
        void *sival_ptr;
    } value = {0};
    if (copy_from_user(&value, value_va, sizeof(value)) < 0) {
        return -EFAULT;
    }

    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
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
    if (sig == 0) {
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return 0; /* permission probe only */
    }
    signal_post_locked(p, target, (unsigned)sig, value.u64, K_SI_QUEUE,
                       cur->pid, cur->uid);
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
            signal_post_locked(p, target, (unsigned)sig, 0, K_SI_USER, cur->pid,
                               cur->uid);
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
                signal_post_locked(p, target, (unsigned)sig, 0, K_SI_USER,
                                   cur->pid, cur->uid);
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
    signal_post_locked(p, target, (unsigned)sig, 0, K_SI_USER, 0u, 0u);
    if (p->stopped) {
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return;
    }
    spin_lock(&g_sched_lock);
    if (target->state == THREAD_WAIT_BLOCKED ||
        target->state == THREAD_SLEEP_BLOCKED ||
        target->state == THREAD_FUTEX_BLOCKED) {
        if (!(target->state == THREAD_WAIT_BLOCKED && target->in_sigsuspend &&
              signal_is_ignored_locked(p, (unsigned)sig))) {
            target->futex_addr = 0;
            target->futex_is_shared = 0u;
            target->futex_has_timeout = 0;
            target->sig_wait_set = 0;
            sched_ready_locked(target);
        }
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
            if (t->has_saved_sigmask) {
                t->blocked_signals = t->saved_sigmask;
                t->has_saved_sigmask = 0u;
            }
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
            if (handler == K_SIG_DFL && (sig == SIGSTOP || sig == SIGTSTP ||
                                         sig == SIGTTIN || sig == SIGTTOU)) {
                pf = spin_lock_irqsave(&g_proc_lock);
                spin_lock(&g_sched_lock);
                int need_ipi = proc_stop_locked(p, t, (int)sig);
                spin_unlock(&g_proc_lock);
                if (need_ipi) {
                    uint32_t total = smp_cpu_count();
                    uint32_t self = arch_cpu_index();
                    for (uint32_t c = 0u; c < total; c++) {
                        if (c != self) {
                            arch_smp_send_resched_ipi(c);
                        }
                    }
                }
                schedule_with_lock(pf);
                continue;
            }
            if (sig == SIGCONT) {
                pf = spin_lock_irqsave(&g_proc_lock);
                proc_resume_locked(p, 1);
                spin_unlock_irqrestore(&g_proc_lock, pf);
                continue;
            }
            if (handler == K_SIG_DFL && signal_is_default_ignore(sig)) {
                continue;
            }
            printk("[sig] pid %u tid %u terminated by signal %u\n", p->pid,
                   t->tid, sig);
            proc_exit_signal((int)sig); /* noreturn */
        }

        sig_frame_t f;
        memcpy(&f, tf, sizeof(trapframe_t));
        f.sig = sig;
        if (t->has_saved_sigmask) {
            f.saved_mask = t->saved_sigmask;
            t->has_saved_sigmask = 0u;
        } else {
            f.saved_mask = t->blocked_signals;
        }

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
