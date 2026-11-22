/**
 * @file     sched.c
 * @brief    Preemptive 256-priority thread scheduler and thread syscalls
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <kernel/arch.h>
#include <kernel/cov.h>
#include <kernel/sched.h>
#include <kernel/ipc.h>
#include <kernel/time.h>
#include <kernel/trapframe.h>
#include <kernel/uaccess.h>
#include <sys/errno.h>
#include <sys/thread.h>
#include <kernel/panic.h>
#include <kernel/printk.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <sys/microkernel.h>
#include <time.h>

/* need:: COMP_PROC_0001 */

/* Maintained by the scheduler for syscall_entry (switch.S). */
extern unsigned long g_syscall_kstack;

/* 256-level priority bitmap scheduler. Each level is a FIFO ready queue; the
 * scheduler picks the highest non-empty level (round-robin within it).
 * Invariant: the RUNNING thread is never in a ready queue. schedule()
 * re-enqueues a preempted RUNNING thread and pops the next one. */
static uint64_t g_ready_bitmap[4]; /* 256 bits, bit set = level non-empty */
static tcb_t *g_ready_head[256];
static tcb_t *g_ready_tail[256];
static tcb_t *g_current;
static int g_sched_active;
static uint64_t g_next_tid = 1;
static uint64_t g_fpu_template[64] __attribute__((aligned(16)));

#define SLICE_TICKS 5

static tcb_t *pick_next(void);

void switch_to(tcb_t *prev, tcb_t *next);
extern void thread_bootstrap(void);

static inline void bitmap_set(unsigned prio) {
    g_ready_bitmap[prio / 64] |= (1ULL << (prio % 64));
}

static inline void bitmap_clear(unsigned prio) {
    g_ready_bitmap[prio / 64] &= ~(1ULL << (prio % 64));
}

static int ready_empty(void) {
    for (int i = 0; i < 4; i++) {
        if (g_ready_bitmap[i]) {
            return 0;
        }
    }
    return 1;
}

static tcb_t *pick_next(void) {
    for (int word = 3; word >= 0; word--) {
        if (!g_ready_bitmap[word]) {
            continue;
        }
        unsigned bit = 63 - __builtin_clzll(g_ready_bitmap[word]);
        unsigned prio = (unsigned)word * 64 + bit;
        return g_ready_head[prio];
    }
    return NULL;
}

void sched_ready(tcb_t *t) {
    unsigned p = t->effective_priority;
    t->state = THREAD_READY;
    t->queue_next = NULL;
    if (g_ready_tail[p]) {
        g_ready_tail[p]->queue_next = t;
    } else {
        g_ready_head[p] = t;
    }
    g_ready_tail[p] = t;
    bitmap_set(p);
}

void sched_unready(tcb_t *t) {
    unsigned p = t->effective_priority;
    tcb_t **pp = &g_ready_head[p];
    tcb_t *prev = NULL;
    while (*pp) {
        if (*pp == t) {
            *pp = t->queue_next;
            if (g_ready_tail[p] == t) {
                g_ready_tail[p] = prev;
            }
            if (!g_ready_head[p]) {
                bitmap_clear(p);
            }
            t->queue_next = NULL;
            return;
        }
        prev = *pp;
        pp = &(*pp)->queue_next;
    }
}

tcb_t *sched_current(void) {
    return g_current;
}

void sched_activate(void) {
    g_sched_active = 1;
}

void sched_block_current(thread_state_t state) {
    tcb_t *t = g_current;
    if (!t) {
        panic("sched: block without current thread");
    }
    t->state = state; /* not RUNNING: schedule() will not re-enqueue us */
    schedule();
    /* Returns when sched_unblock() made this thread runnable again and the
     * scheduler picked it; the caller resumes on its own kernel stack. */
}

void sched_unblock(tcb_t *t) {
    if (t->state == THREAD_RUNNING || t->state == THREAD_READY) {
        return; /* already runnable; callers must not pre-mark READY */
    }
    sched_ready(t); /* sets t->state = THREAD_READY and enqueues */
    tcb_t *cur = g_current;
    if (cur && cur->state == THREAD_RUNNING &&
        t->effective_priority > cur->effective_priority) {
        schedule(); /* preempt: the woken thread outranks us */
    }
}

/* switch.S accesses the TCB at fixed offsets; fail the build if they drift. */
_Static_assert(offsetof(tcb_t, kernel_rsp) == 32, "switch.S TCB_KRSP mismatch");
_Static_assert(offsetof(tcb_t, fpu_area) == 40, "switch.S TCB_FPU mismatch");

void sched_init(void) {
    arch_fpu_init_template((uint8_t *)g_fpu_template);
    printk("[sched] 256-level bitmap scheduler ready\n");
}

static void apply_switch(tcb_t *next) {
    if (g_current && next->proc &&
        g_current->proc->pml4_phys != next->proc->pml4_phys) {
        vmm_set_cr3(next->proc->pml4_phys);
    } else if (!g_current && next->proc) {
        vmm_set_cr3(next->proc->pml4_phys);
    }
    arch_set_kernel_stack(next->kstack_top);
    g_syscall_kstack = next->kstack_top;
    arch_set_tls_base(next->fs_base);
    next->state = THREAD_RUNNING;
    next->slice_ticks = 0;
    g_current = next;
}

void schedule(void) {
    tcb_t *next = pick_next();
    while (!next) {
        if (g_current && g_current->state == THREAD_RUNNING) {
            return; /* nothing else ready: keep running */
        }
        /* Nothing runnable and the current thread cannot continue (blocked or
         * reaped): enable interrupts and halt until a timer/IRQ wakes runnable
         * work. */
        arch_cpu_idle_wait();
        next = pick_next();
    }

    /* Strict priority scheduling: never preempt a RUNNING thread for a
     * lower-priority ready thread on a round-robin tick. */
    if (g_current && g_current->state == THREAD_RUNNING &&
        next->effective_priority < g_current->effective_priority) {
        g_current->slice_ticks = 0;
        return;
    }

    /* Pop `next` from its level queue. */
    unsigned p = next->effective_priority;
    g_ready_head[p] = next->queue_next;
    if (!g_ready_head[p]) {
        g_ready_tail[p] = NULL;
        bitmap_clear(p);
    }
    next->queue_next = NULL;

    if (next == g_current) {
        g_current->state = THREAD_RUNNING;
        g_current->slice_ticks = 0;
        return;
    }

    /* Re-enqueue the preempted current (READY while it waits). */
    if (g_current && g_current->state == THREAD_RUNNING) {
        sched_ready(g_current);
    }

    tcb_t *prev = g_current;
    apply_switch(next);

    if (prev) {
        switch_to(prev, next);
    } else {
        arch_first_switch(next->kernel_rsp);
    }
}

void sched_tick(void) {
    if (!g_sched_active) {
        return;
    }
    tcb_t *cur = g_current;
    if (!cur || cur->state != THREAD_RUNNING) {
        return;
    }
    if (cur->proc) {
        cur->proc->utime_ticks++;
    }
    cur->slice_ticks++;
    if (cur->slice_ticks >= SLICE_TICKS && !ready_empty()) {
        schedule();
    }
}

long sys_sched_yield(void) {
    tcb_t *cur = g_current;
    if (!cur || cur->state != THREAD_RUNNING) {
        return 0;
    }
    if (!ready_empty()) {
        schedule();
    }
    return 0;
}

long sys_proc_times(uint64_t ubuf) {
    pcb_t *p = proc_current();
    if (!p) {
        return -EPERM;
    }
    if (!ubuf) {
        return -EFAULT;
    }
    struct __k_tms kt = {
        .utime = (int64_t)p->utime_ticks,
        .stime = (int64_t)p->stime_ticks,
        .cutime = (int64_t)p->cutime_ticks,
        .cstime = (int64_t)p->cstime_ticks,
    };
    if (copy_to_user(ubuf, &kt, sizeof(kt)) < 0) {
        return -EFAULT;
    }
    return (long)(time_monotonic_ns() / 1000000ULL);
}

/* ---- processes ---- */

static pcb_t *g_proc_list;
static uint32_t g_next_pid = 1;

uint32_t proc_alloc_pid(void) {
    return g_next_pid++;
}

pcb_t *proc_find(uint32_t pid) {
    for (pcb_t *p = g_proc_list; p; p = p->next) {
        if (p->pid == pid && p->state != PROC_DEAD) {
            return p;
        }
    }
    return NULL;
}

pcb_t *proc_current(void) {
    return g_current ? g_current->proc : NULL;
}

pcb_t *proc_list_head(void) {
    return g_proc_list;
}

#define CONN_TABLE_SIZE 32

pcb_t *proc_create(pcb_t *parent) {
    pcb_t *p = kzalloc(sizeof(pcb_t));
    if (!p) {
        return NULL;
    }
    p->pid = proc_alloc_pid();
    p->parent_pid = parent ? parent->pid : 0;
    p->state = PROC_ALIVE;
    p->pml4_phys = vmm_create_user_pml4();
    if (!p->pml4_phys) {
        kfree(p);
        return NULL;
    }
    p->conn_table = kzalloc(CONN_TABLE_SIZE * sizeof(*p->conn_table));
    if (!p->conn_table) {
        kfree(p);
        return NULL;
    }
    p->conn_count = CONN_TABLE_SIZE;
    p->fds[0].kind = FD_KIND_CONSOLE;
    p->fds[0].status_flags = 0; /* O_RDONLY */
    p->fds[1].kind = FD_KIND_CONSOLE;
    p->fds[1].status_flags = 1; /* O_WRONLY */
    p->fds[2].kind = FD_KIND_CONSOLE;
    p->fds[2].status_flags = 1; /* O_WRONLY */
    p->next = g_proc_list;
    g_proc_list = p;
    return p;
}

tcb_t *thread_create_embryo(pcb_t *p, uint8_t priority) {
    tcb_t *t = kzalloc(sizeof(tcb_t));
    uint8_t *kstack = kmalloc(KERNEL_STACK_SIZE);
    uint8_t *fpu_raw = kmalloc(512 + 16);
    if (!t || !kstack || !fpu_raw) {
        panic("thread_create: out of kernel memory");
    }
    uint8_t *fpu = (uint8_t *)(((uintptr_t)fpu_raw + 15) & ~(uintptr_t)15);
    memcpy(fpu, g_fpu_template, 512);

    t->tid = (uint32_t)g_next_tid++;
    t->pid = p->pid;
    t->state = THREAD_EMBRYO;
    t->base_priority = priority;
    t->effective_priority = priority;
    t->proc = p;
    t->kstack_top = (uint64_t)kstack + KERNEL_STACK_SIZE;
    t->fpu_area = fpu;
    t->fpu_raw = fpu_raw;

    t->proc_next = p->threads;
    p->threads = t;
    p->nthreads++;
    return t;
}

/* Synthesize the initial Ring 3 frame at the top of the kernel stack:
 * [switch_to frame: 6 callee slots + bootstrap address][trapframe]. */
void thread_arm(tcb_t *t, uint64_t entry_rip, uint64_t arg, uint64_t user_rsp) {
    uint64_t *sp = (uint64_t *)(t->kstack_top - 56 - sizeof(trapframe_t));
    t->kernel_rsp = (uint64_t)sp;

    sp[6] = (uint64_t)thread_bootstrap;

    trapframe_t *tf = (trapframe_t *)(sp + 7);
    memset(tf, 0, sizeof(*tf));
    arch_tf_init_user(tf, entry_rip, arg, user_rsp, 0);

    t->start_rip = entry_rip;
    t->start_arg = arg;
    t->start_rsp = user_rsp;
    sched_ready(t);
}

void thread_arm_fork(tcb_t *t, const trapframe_t *tf) {
    arch_fpu_save_current(t->fpu_area);

    uint64_t *sp = (uint64_t *)(t->kstack_top - 56 - sizeof(trapframe_t));
    t->kernel_rsp = (uint64_t)sp;
    sp[6] = (uint64_t)thread_bootstrap;

    trapframe_t *ctf = (trapframe_t *)(sp + 7);
    arch_tf_setup_fork_child(ctf, tf, t->iopl);

    t->start_rip = ctf->rip;
    t->start_arg = 0;
    t->start_rsp = arch_tf_user_sp(ctf);
    sched_ready(t);
}

tcb_t *thread_create(pcb_t *p, uint64_t entry_rip, uint64_t user_rsp,
                     uint8_t priority) {
    tcb_t *t = thread_create_embryo(p, priority);
    thread_arm(t, entry_rip, 0, user_rsp);
    return t;
}

tcb_t *thread_find(pcb_t *p, uint32_t tid) {
    for (tcb_t *t = p->threads; t; t = t->proc_next) {
        if (t->tid == tid && t->state != THREAD_ZOMBIE) {
            return t;
        }
    }
    return NULL;
}

static void reap_thread(tcb_t *t) {
    sched_unready(t);
    ipc_detach_thread(t); /* drop any channel queues / in-flight messages */
    t->state = THREAD_ZOMBIE;
    /* Kernel stack and FPU area are reclaimed when the process is reaped. */
}

void thread_exit_current(int status) {
    tcb_t *t = g_current;
    pcb_t *p = t->proc;
    reap_thread(t);
    p->nthreads--;
    if (p->nthreads == 0) {
        cov_dump_proc(p);
        time_cleanup_proc(p->pid);
        proc_close_fds(p, 0);
        p->state = PROC_ZOMBIE;
        p->exit_code = status;
        printk("[proc] pid %u became zombie (exit %d)\n", p->pid, status);
        pcb_t *parent = p->parent_pid ? proc_find(p->parent_pid) : NULL;
        if (parent) {
            for (tcb_t *wt = parent->threads; wt; wt = wt->proc_next) {
                if (wt->state == THREAD_WAIT_BLOCKED) {
                    sched_ready(wt);
                }
            }
        }
    }
    schedule();
    panic("thread_exit_current: returned to zombie");
}

void proc_kill_other_threads(pcb_t *p, tcb_t *keep) {
    for (tcb_t *x = p->threads; x; x = x->proc_next) {
        if (x != keep && x->state != THREAD_ZOMBIE) {
            reap_thread(x);
        }
    }
    p->nthreads = 1;
}

void proc_reap_zombie_threads(pcb_t *p) {
    if (!p) {
        return;
    }
    tcb_t **pp = &p->threads;
    while (*pp) {
        tcb_t *t = *pp;
        if (t != g_current && t->state == THREAD_ZOMBIE) {
            *pp = t->proc_next;
            if (t->user_stack_base && p->pml4_phys) {
                for (uint64_t off = 0; off < 0x10000ULL; off += 4096ULL) {
                    uint64_t phys = vmm_unmap_page_to(p->pml4_phys,
                                                      t->user_stack_base + off);
                    if (phys) {
                        pmm_free_frame(phys);
                    }
                    if (p->pml4_phys == vmm_get_cr3()) {
                        vmm_invlpg(t->user_stack_base + off);
                    }
                }
            }
            if (t->kstack_top) {
                kfree((void *)(t->kstack_top - KERNEL_STACK_SIZE));
            }
            if (t->fpu_raw) {
                kfree(t->fpu_raw);
            }
            kfree(t);
            continue;
        }
        pp = &t->proc_next;
    }
}

void proc_reap_zombie(pcb_t *zombie) {
    if (!zombie) {
        return;
    }
    tcb_t *t = zombie->threads;
    while (t) {
        tcb_t *next = t->proc_next;
        if (t->kstack_top) {
            kfree((void *)(t->kstack_top - KERNEL_STACK_SIZE));
        }
        if (t->fpu_raw) {
            kfree(t->fpu_raw);
        }
        kfree(t);
        t = next;
    }
    zombie->threads = NULL;
    if (zombie->conn_table) {
        kfree(zombie->conn_table);
        zombie->conn_table = NULL;
    }
    pcb_t **pp = &g_proc_list;
    while (*pp) {
        if (*pp == zombie) {
            *pp = zombie->next;
            break;
        }
        pp = &(*pp)->next;
    }
    kfree(zombie);
}

void proc_exit_current(int status) {
    tcb_t *t = g_current;
    pcb_t *p = t->proc;
    for (tcb_t *x = p->threads; x; x = x->proc_next) {
        if (x != t && x->state != THREAD_ZOMBIE) {
            reap_thread(x);
        }
    }
    p->nthreads = 1; /* only the current thread remains to reap below */
    thread_exit_current(status);
}

void proc_exit_signal(int sig) {
    tcb_t *t = g_current;
    t->proc->exit_signal = sig;
    proc_exit_current(128 + sig);
}

/* ---- Milestone 4 thread syscalls (caller is current) ---- */

long sys_thread_create(uint64_t prio) {
    pcb_t *p = proc_current();
    if (!p) {
        return -EPERM;
    }
    if (prio > 255) {
        return -EINVAL;
    }
    proc_reap_zombie_threads(p);
    tcb_t *t = thread_create_embryo(p, (uint8_t)prio);
    return t ? (long)t->tid : -ENOMEM;
}

long sys_thread_reset(uint64_t tid, uint64_t entry, uint64_t arg,
                      uint64_t stack_top) {
    pcb_t *p = proc_current();
    if (!p) {
        return -EPERM;
    }
    tcb_t *t = thread_find(p, (uint32_t)tid);
    if (!t) {
        return -ESRCH;
    }
    if (t->state != THREAD_EMBRYO) {
        return -EINVAL; /* only unarmed threads can be (re)armed */
    }
    if (!stack_top) {
        stack_top = proc_user_stack_alloc(p);
        if (!stack_top) {
            return -ENOMEM;
        }
        t->user_stack_base = stack_top - 0x10000ULL;
        /* Pre-point FS.base at the zeroed stack bottom so %fs:0 reads NULL
         * safely even for raw ThreadSpawn workers before TLS init. */
        if (!t->fs_base) {
            t->fs_base = t->user_stack_base;
        }
    }
    /* Entry point and stack must live in the user region. */
    if (entry < 0x400000ULL || entry >= 0x80000000ULL ||
        stack_top < 0x400000ULL || stack_top > 0x80000000ULL) {
        return -EINVAL;
    }
    /* SysV AMD64 C function entry expects RSP % 16 == 8 (synthetic return
     * slot). */
    uint64_t entry_rsp = (stack_top & ~15ULL) - 8;
    thread_arm(t, entry, arg, entry_rsp);
    return 0;
}

long sys_thread_get_ctx(uint64_t tid, uint64_t ubuf) {
    pcb_t *p = proc_current();
    tcb_t *t;
    if (!p) {
        return -EPERM;
    }
    if (tid == 0) {
        t = sched_current();
    } else {
        t = thread_find(p, (uint32_t)tid);
    }
    if (!t || t->proc != p) {
        return -ESRCH;
    }
    thread_info_t ti = {
        .tid = t->tid,
        .state = (uint32_t)t->state,
        .prio = t->effective_priority,
        .entry = t->start_rip,
        .arg = t->start_arg,
        .stack_top = t->start_rsp,
    };
    return copy_to_user(ubuf, &ti, sizeof(ti)) < 0 ? -EFAULT : 0;
}

long sys_thread_set_fsbase(uint64_t base) {
    tcb_t *t = sched_current();
    if (!t) {
        return -EPERM;
    }
    if (base >= 0x0000800000000000ULL) {
        return -EINVAL;
    }
    t->fs_base = base;
    arch_set_tls_base(base);
    return 0;
}

long sys_futex(uint64_t uaddr, uint64_t op, uint64_t val, uint64_t timeout_va,
               uint64_t clock_id) {
    tcb_t *cur = sched_current();
    pcb_t *p = cur ? cur->proc : NULL;
    if (!cur || !p) {
        return -EPERM;
    }
    if ((uaddr & 3ULL) != 0 || uaddr >= 0x00007FFFFFFFE000ULL) {
        return -EINVAL;
    }

    if (op == FUTEX_WAIT) {
        uint32_t cur_val = 0;
        if (copy_from_user(&cur_val, uaddr, sizeof(cur_val)) < 0) {
            return -EFAULT;
        }
        if (cur_val != (uint32_t)val) {
            return -EAGAIN;
        }
        if (timeout_va != 0) {
            if (clock_id != CLOCK_REALTIME && clock_id != CLOCK_MONOTONIC) {
                return -EINVAL;
            }
            struct timespec ts = {0};
            if (copy_from_user(&ts, timeout_va, sizeof(ts)) < 0) {
                return -EFAULT;
            }
            if (ts.tv_sec < 0 || ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000L) {
                return -EINVAL;
            }
            uint64_t deadline =
                (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
            if (time_clock_now_ns((int)clock_id) >= deadline) {
                return -ETIMEDOUT;
            }
            cur->futex_has_timeout = 1;
            cur->sleep_clock_id = (uint8_t)clock_id;
            cur->sleep_deadline_ns = deadline;
        } else {
            cur->futex_has_timeout = 0;
        }

        cur->futex_addr = uaddr;
        cur->futex_woken = 0;
        sched_block_current(THREAD_FUTEX_BLOCKED);
        cur->futex_addr = 0;
        cur->futex_has_timeout = 0;
        if (cur->futex_woken) {
            return 0;
        }
        if (cur->pending_signals & ~cur->blocked_signals) {
            return -EINTR;
        }
        return -ETIMEDOUT;
    }

    if (op == FUTEX_WAKE) {
        uint64_t woken = 0;
        int need_preempt = 0;
        for (tcb_t *t = p->threads; t && woken < val; t = t->proc_next) {
            if (t->state == THREAD_FUTEX_BLOCKED && t->futex_addr == uaddr) {
                t->futex_addr = 0;
                t->futex_has_timeout = 0;
                t->futex_woken = 1;
                sched_ready(t);
                if (t->effective_priority > cur->effective_priority) {
                    need_preempt = 1;
                }
                woken++;
            }
        }
        if (need_preempt) {
            schedule();
        }
        return (long)woken;
    }

    return -EINVAL;
}
