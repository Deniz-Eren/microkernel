/**
 * @file     time.c
 * @brief    SMP-safe POSIX.1-2024 clocks, nanosleep, alarm, and timers
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <kernel/time.h>
#include <kernel/sched.h>
#include <kernel/signal.h>
#include <kernel/spinlock.h>
#include <kernel/uaccess.h>
#include <limits.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <time.h>

/* need:: COMP_PROC_0004 COMP_PROC_0008 */

#define TICK_NS 1000000ULL /* 1 ms LAPIC periodic tick */
#define NSEC_PER_SEC 1000000000ULL
#define MAX_POSIX_TIMERS 64

typedef struct {
    int used;
    int armed;
    uint32_t owner_pid;
    clockid_t clock_id;
    int notify;
    int signo;
    uint64_t expire_ns;
    uint64_t interval_ns;
    int overrun;
} ktimer_t;

static spinlock_t g_time_lock = SPINLOCK_INIT;
static volatile uint64_t g_mono_ns;
static volatile int64_t g_realtime_offset_ns;
static ktimer_t g_timers[MAX_POSIX_TIMERS];

static int valid_clock(uint64_t clock_id) {
    return clock_id == CLOCK_REALTIME || clock_id == CLOCK_MONOTONIC;
}

static uint64_t clock_now_ns(clockid_t clock_id) {
    uint64_t mono = __atomic_load_n(&g_mono_ns, __ATOMIC_ACQUIRE);
    if (clock_id == CLOCK_REALTIME) {
        int64_t off = __atomic_load_n(&g_realtime_offset_ns, __ATOMIC_ACQUIRE);
        int64_t rt = (int64_t)mono + off;
        return rt > 0 ? (uint64_t)rt : 0ULL;
    }
    return mono;
}

static int valid_timespec(const struct timespec *ts) {
    return ts->tv_sec >= 0 && ts->tv_nsec >= 0 &&
           (uint64_t)ts->tv_nsec < NSEC_PER_SEC;
}

static uint64_t ts_to_ns(const struct timespec *ts) {
    return (uint64_t)ts->tv_sec * NSEC_PER_SEC + (uint64_t)ts->tv_nsec;
}

static struct timespec ns_to_ts(uint64_t ns) {
    struct timespec ts;
    ts.tv_sec = (time_t)(ns / NSEC_PER_SEC);
    ts.tv_nsec = (long)(ns % NSEC_PER_SEC);
    return ts;
}

uint64_t time_monotonic_ns(void) {
    return __atomic_load_n(&g_mono_ns, __ATOMIC_ACQUIRE);
}

uint64_t time_clock_now_ns(int clock_id) {
    return clock_now_ns((clockid_t)clock_id);
}

void time_cleanup_proc(uint32_t pid) {
    uint64_t tf = spin_lock_irqsave(&g_time_lock);
    for (int i = 0; i < MAX_POSIX_TIMERS; i++) {
        if (g_timers[i].used && g_timers[i].owner_pid == pid) {
            g_timers[i].used = 0;
            g_timers[i].armed = 0;
        }
    }
    spin_unlock_irqrestore(&g_time_lock, tf);
}

/* Called on every 1 ms BSP LAPIC timer interrupt (before sched_tick). */
void time_tick(void) {
    uint64_t mono = __atomic_add_fetch(&g_mono_ns, TICK_NS, __ATOMIC_ACQ_REL);

    struct {
        uint32_t pid;
        int signo;
    } sigs[32];
    size_t nsigs = 0;

    /* Wake sleeping threads whose deadline has expired. */
    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    spin_lock(&g_sched_lock);
    for (pcb_t *p = proc_list_head(); p; p = p->next) {
        if (p->state != PROC_ALIVE) {
            continue;
        }
        if (p->alarm_expire_ns != 0ULL && mono >= p->alarm_expire_ns) {
            p->alarm_expire_ns = 0ULL;
            if (nsigs < 32u) {
                sigs[nsigs].pid = p->pid;
                sigs[nsigs].signo = SIGALRM;
                nsigs++;
            }
        }
        for (tcb_t *t = p->threads; t; t = t->proc_next) {
            if (t->state == THREAD_SLEEP_BLOCKED) {
                uint64_t now = clock_now_ns((clockid_t)t->sleep_clock_id);
                if (now >= t->sleep_deadline_ns) {
                    sched_ready_locked(t);
                }
            } else if (t->state == THREAD_FUTEX_BLOCKED &&
                       t->futex_has_timeout) {
                uint64_t now = clock_now_ns((clockid_t)t->sleep_clock_id);
                if (now >= t->sleep_deadline_ns) {
                    t->futex_addr = 0;
                    t->futex_has_timeout = 0;
                    sched_ready_locked(t);
                }
            }
        }
    }
    spin_unlock(&g_sched_lock);
    spin_unlock_irqrestore(&g_proc_lock, pf);

    /* Fire expired POSIX interval timers. */
    uint64_t tf = spin_lock_irqsave(&g_time_lock);
    for (int i = 0; i < MAX_POSIX_TIMERS; i++) {
        ktimer_t *tm = &g_timers[i];
        if (!tm->used || !tm->armed) {
            continue;
        }
        uint64_t now = clock_now_ns(tm->clock_id);
        if (now < tm->expire_ns) {
            continue;
        }
        if (tm->interval_ns > 0) {
            uint64_t missed = (now - tm->expire_ns) / tm->interval_ns;
            tm->expire_ns += (missed + 1ULL) * tm->interval_ns;
            if (missed > 0) {
                uint64_t ov = (uint64_t)tm->overrun + missed;
                tm->overrun = ov > (uint64_t)INT_MAX ? INT_MAX : (int)ov;
            }
        } else {
            tm->armed = 0;
        }
        if (tm->notify == SIGEV_SIGNAL && nsigs < 32u) {
            sigs[nsigs].pid = tm->owner_pid;
            sigs[nsigs].signo = tm->signo;
            nsigs++;
        }
    }
    spin_unlock_irqrestore(&g_time_lock, tf);

    for (size_t i = 0; i < nsigs; i++) {
        signal_post_to_proc(sigs[i].pid, sigs[i].signo);
    }
}

long sys_clock_gettime(uint64_t clock_id, uint64_t tp_va) {
    if (!valid_clock(clock_id)) {
        return -EINVAL;
    }
    struct timespec ts = ns_to_ts(clock_now_ns((clockid_t)clock_id));
    return copy_to_user(tp_va, &ts, sizeof(ts)) < 0 ? -EFAULT : 0;
}

long sys_clock_settime(uint64_t clock_id, uint64_t tp_va) {
    if (clock_id != CLOCK_REALTIME) {
        return -EINVAL; /* CLOCK_MONOTONIC is read-only per POSIX */
    }
    struct timespec ts = {0};
    if (copy_from_user(&ts, tp_va, sizeof(ts)) < 0) {
        return -EFAULT;
    }
    if (!valid_timespec(&ts)) {
        return -EINVAL;
    }
    uint64_t tf = spin_lock_irqsave(&g_time_lock);
    int64_t target_ns = (int64_t)ts_to_ns(&ts);
    int64_t off = target_ns - (int64_t)time_monotonic_ns();
    __atomic_store_n(&g_realtime_offset_ns, off, __ATOMIC_RELEASE);
    spin_unlock_irqrestore(&g_time_lock, tf);
    return 0;
}

long sys_clock_getres(uint64_t clock_id, uint64_t res_va) {
    if (!valid_clock(clock_id)) {
        return -EINVAL;
    }
    if (res_va != 0) {
        struct timespec res = {.tv_sec = 0, .tv_nsec = (long)TICK_NS};
        if (copy_to_user(res_va, &res, sizeof(res)) < 0) {
            return -EFAULT;
        }
    }
    return 0;
}

long sys_clock_nanosleep(uint64_t clock_id, uint64_t flags, uint64_t rqtp_va,
                         uint64_t rmtp_va) {
    if (!valid_clock(clock_id) || (flags & ~(uint64_t)TIMER_ABSTIME)) {
        return -EINVAL;
    }
    struct timespec rqtp = {0};
    if (copy_from_user(&rqtp, rqtp_va, sizeof(rqtp)) < 0) {
        return -EFAULT;
    }
    if (!valid_timespec(&rqtp)) {
        return -EINVAL;
    }

    uint64_t now = clock_now_ns((clockid_t)clock_id);
    uint64_t req_ns = ts_to_ns(&rqtp);
    uint64_t deadline = (flags & TIMER_ABSTIME) ? req_ns : (now + req_ns);

    if (deadline <= now) {
        return 0;
    }

    tcb_t *cur = sched_current();
    if (!cur) {
        return -EPERM;
    }

    for (;;) {
        uint64_t sf = spin_lock_irqsave(&g_sched_lock);
        uint64_t cur_now = clock_now_ns((clockid_t)clock_id);
        if (cur_now >= deadline) {
            spin_unlock_irqrestore(&g_sched_lock, sf);
            return 0;
        }
        if (__atomic_load_n(&cur->pending_signals, __ATOMIC_ACQUIRE) &
            ~cur->blocked_signals) {
            spin_unlock_irqrestore(&g_sched_lock, sf);
            if (!(flags & TIMER_ABSTIME) && rmtp_va != 0) {
                uint64_t rem = deadline - clock_now_ns((clockid_t)clock_id);
                struct timespec rts = ns_to_ts(rem);
                (void)copy_to_user(rmtp_va, &rts, sizeof(rts));
            }
            return -EINTR;
        }
        cur->sleep_clock_id = (uint8_t)clock_id;
        cur->sleep_deadline_ns = deadline;
        cur->state = THREAD_SLEEP_BLOCKED;
        schedule_with_lock(sf);
    }
}

long sys_timer_create(uint64_t clock_id, uint64_t sevp_va, uint64_t tid_va) {
    pcb_t *p = proc_current();
    if (!p) {
        return -EPERM;
    }
    if (!valid_clock(clock_id)) {
        return -EINVAL;
    }
    int notify = SIGEV_SIGNAL;
    int signo = SIGALRM;
    if (sevp_va != 0) {
        struct sigevent ev = {0};
        if (copy_from_user(&ev, sevp_va, sizeof(ev)) < 0) {
            return -EFAULT;
        }
        if (ev.sigev_notify != SIGEV_NONE && ev.sigev_notify != SIGEV_SIGNAL) {
            return -EINVAL;
        }
        notify = ev.sigev_notify;
        if (notify == SIGEV_SIGNAL) {
            if (ev.sigev_signo < 1 || ev.sigev_signo >= NSIG) {
                return -EINVAL;
            }
            signo = ev.sigev_signo;
        }
    }

    int slot = -1;
    uint64_t tf = spin_lock_irqsave(&g_time_lock);
    for (int i = 0; i < MAX_POSIX_TIMERS; i++) {
        if (!g_timers[i].used) {
            ktimer_t *tm = &g_timers[i];
            tm->used = 1;
            tm->armed = 0;
            tm->owner_pid = p->pid;
            tm->clock_id = (clockid_t)clock_id;
            tm->notify = notify;
            tm->signo = signo;
            tm->expire_ns = 0;
            tm->interval_ns = 0;
            tm->overrun = 0;
            slot = i;
            break;
        }
    }
    spin_unlock_irqrestore(&g_time_lock, tf);

    if (slot < 0) {
        return -EAGAIN;
    }
    timer_t tid = (timer_t)slot;
    if (copy_to_user(tid_va, &tid, sizeof(tid)) < 0) {
        tf = spin_lock_irqsave(&g_time_lock);
        g_timers[slot].used = 0;
        spin_unlock_irqrestore(&g_time_lock, tf);
        return -EFAULT;
    }
    return 0;
}

static ktimer_t *lookup_timer_locked(pcb_t *p, uint64_t timerid) {
    if (!p || timerid >= MAX_POSIX_TIMERS) {
        return NULL;
    }
    ktimer_t *tm = &g_timers[timerid];
    if (!tm->used || tm->owner_pid != p->pid) {
        return NULL;
    }
    return tm;
}

long sys_timer_delete(uint64_t timerid) {
    pcb_t *p = proc_current();
    uint64_t tf = spin_lock_irqsave(&g_time_lock);
    ktimer_t *tm = lookup_timer_locked(p, timerid);
    if (!tm) {
        spin_unlock_irqrestore(&g_time_lock, tf);
        return -EINVAL;
    }
    tm->used = 0;
    tm->armed = 0;
    spin_unlock_irqrestore(&g_time_lock, tf);
    return 0;
}

long sys_timer_settime(uint64_t timerid, uint64_t flags, uint64_t val_va,
                       uint64_t oval_va) {
    pcb_t *p = proc_current();
    if (!p || (flags & ~(uint64_t)TIMER_ABSTIME)) {
        return -EINVAL;
    }
    struct itimerspec nv = {0};
    if (copy_from_user(&nv, val_va, sizeof(nv)) < 0) {
        return -EFAULT;
    }
    if (!valid_timespec(&nv.it_value) || !valid_timespec(&nv.it_interval)) {
        return -EINVAL;
    }

    struct itimerspec ov = {0};
    uint64_t tf = spin_lock_irqsave(&g_time_lock);
    ktimer_t *tm = lookup_timer_locked(p, timerid);
    if (!tm) {
        spin_unlock_irqrestore(&g_time_lock, tf);
        return -EINVAL;
    }
    uint64_t now = clock_now_ns(tm->clock_id);
    if (oval_va != 0 && tm->armed) {
        uint64_t rem = (tm->expire_ns > now) ? (tm->expire_ns - now) : 0ULL;
        ov.it_value = ns_to_ts(rem);
        ov.it_interval = ns_to_ts(tm->interval_ns);
    }
    uint64_t val_ns = ts_to_ns(&nv.it_value);
    uint64_t int_ns = ts_to_ns(&nv.it_interval);
    tm->overrun = 0;
    if (val_ns == 0) {
        tm->armed = 0;
        tm->expire_ns = 0;
        tm->interval_ns = 0;
    } else {
        tm->expire_ns = (flags & TIMER_ABSTIME) ? val_ns : (now + val_ns);
        tm->interval_ns = int_ns;
        tm->armed = 1;
    }
    spin_unlock_irqrestore(&g_time_lock, tf);

    if (oval_va != 0 && copy_to_user(oval_va, &ov, sizeof(ov)) < 0) {
        return -EFAULT;
    }
    return 0;
}

long sys_timer_gettime(uint64_t timerid, uint64_t val_va) {
    pcb_t *p = proc_current();
    struct itimerspec cur = {0};
    uint64_t tf = spin_lock_irqsave(&g_time_lock);
    ktimer_t *tm = lookup_timer_locked(p, timerid);
    if (!tm) {
        spin_unlock_irqrestore(&g_time_lock, tf);
        return -EINVAL;
    }
    if (tm->armed) {
        uint64_t now = clock_now_ns(tm->clock_id);
        uint64_t rem = (tm->expire_ns > now) ? (tm->expire_ns - now) : 0ULL;
        cur.it_value = ns_to_ts(rem);
        cur.it_interval = ns_to_ts(tm->interval_ns);
    }
    spin_unlock_irqrestore(&g_time_lock, tf);
    return copy_to_user(val_va, &cur, sizeof(cur)) < 0 ? -EFAULT : 0;
}

long sys_timer_getoverrun(uint64_t timerid) {
    pcb_t *p = proc_current();
    uint64_t tf = spin_lock_irqsave(&g_time_lock);
    ktimer_t *tm = lookup_timer_locked(p, timerid);
    if (!tm) {
        spin_unlock_irqrestore(&g_time_lock, tf);
        return -EINVAL;
    }
    int ov = tm->overrun;
    spin_unlock_irqrestore(&g_time_lock, tf);
    return ov;
}

long sys_alarm(uint64_t seconds) {
    pcb_t *p = proc_current();
    if (!p) {
        return -EPERM;
    }
    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    uint64_t now = time_monotonic_ns();
    uint64_t rem_sec = 0ULL;
    if (p->alarm_expire_ns > now) {
        uint64_t rem_ns = p->alarm_expire_ns - now;
        rem_sec = (rem_ns + NSEC_PER_SEC - 1ULL) / NSEC_PER_SEC;
        if (rem_sec == 0ULL) {
            rem_sec = 1ULL;
        }
    }
    if (seconds == 0ULL) {
        p->alarm_expire_ns = 0ULL;
    } else {
        uint64_t cap = (~0ULL - now) / NSEC_PER_SEC;
        uint64_t clamped = seconds > cap ? cap : seconds;
        p->alarm_expire_ns = now + clamped * NSEC_PER_SEC;
    }
    spin_unlock_irqrestore(&g_proc_lock, pf);
    return (long)rem_sec;
}
