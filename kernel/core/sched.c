/**
 * @file     sched.c
 * @brief    SMP preemptive 256-priority thread scheduler and thread syscalls
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <kernel/arch.h>
#include <kernel/cov.h>
#include <kernel/ipc.h>
#include <kernel/kheap.h>
#include <kernel/panic.h>
#include <kernel/pmm.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <kernel/smp.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/time.h>
#include <kernel/trapframe.h>
#include <kernel/uaccess.h>
#include <kernel/vmm.h>
#include <sys/errno.h>
#include <sys/microkernel.h>
#include <sys/thread.h>
#include <time.h>

/* need:: COMP_PROC_0001 COMP_PROC_0008 */

spinlock_t g_proc_lock = SPINLOCK_INIT;
spinlock_t g_sched_lock = SPINLOCK_INIT;
static spinlock_t g_futex_lock = SPINLOCK_INIT;

/* Global 256-level priority bitmap scheduler shared across all SMP cores. Each
 * level is a FIFO ready queue; the scheduler picks the highest non-empty level
 * (round-robin within it). Invariant: a RUNNING or on_cpu thread is never in a
 * ready queue; outgoing threads are re-enqueued in sched_post_switch() after
 * switch_to() has left their stack. */
static uint64_t g_ready_bitmap[4];
static tcb_t *g_ready_head[256];
static tcb_t *g_ready_tail[256];
static volatile int g_sched_active;
static uint32_t g_next_tid = 1u;
static uint8_t *g_fpu_template;
static size_t g_fpu_size;
static size_t g_fpu_align;

#define SLICE_TICKS 5

void switch_to(tcb_t *prev, tcb_t *next);
extern void thread_bootstrap(void);

static inline void bitmap_set(unsigned prio) {
    g_ready_bitmap[prio / 64] |= (1ULL << (prio % 64));
}

static inline void bitmap_clear(unsigned prio) {
    g_ready_bitmap[prio / 64] &= ~(1ULL << (prio % 64));
}

static int ready_empty_locked(void) {
    return (g_ready_bitmap[0] | g_ready_bitmap[1] | g_ready_bitmap[2] |
            g_ready_bitmap[3]) == 0ULL;
}

static tcb_t *pick_next_locked(void) {
    for (int word = 3; word >= 0; word--) {
        uint64_t mask = g_ready_bitmap[word];
        while (mask) {
            unsigned bit = 63u - (unsigned)__builtin_clzll(mask);
            unsigned prio = (unsigned)word * 64u + bit;
            for (tcb_t *t = g_ready_head[prio]; t; t = t->queue_next) {
                if (__atomic_load_n(&t->on_cpu, __ATOMIC_ACQUIRE) == 0u) {
                    return t;
                }
            }
            mask &= ~(1ULL << bit);
        }
    }
    return NULL;
}

static void ready_remove_locked(tcb_t *t) {
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

static void kick_other_cpu_locked(uint8_t prio) {
    uint32_t total = smp_cpu_count();
    if (total <= 1u || !__atomic_load_n(&g_sched_active, __ATOMIC_ACQUIRE)) {
        return;
    }
    uint32_t self_idx = arch_cpu_index();
    uint32_t target = SMP_MAX_CPUS;
    uint8_t lowest_prio = prio;

    for (uint32_t i = 0; i < total; i++) {
        if (i == self_idx) {
            continue;
        }
        cpu_local_t *c = smp_cpu(i);
        if (!c || !__atomic_load_n(&c->online, __ATOMIC_ACQUIRE)) {
            continue;
        }
        if (__atomic_load_n(&c->is_idle, __ATOMIC_ACQUIRE)) {
            target = i;
            break;
        }
        tcb_t *ct = c->current;
        if (ct && !ct->is_idle && ct->effective_priority < lowest_prio) {
            lowest_prio = ct->effective_priority;
            target = i;
        }
    }
    if (target < SMP_MAX_CPUS) {
        arch_smp_send_resched_ipi(target);
    }
}

void sched_ready_locked(tcb_t *t) {
    if (t->is_idle || t->state == THREAD_ZOMBIE) {
        return;
    }
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
    kick_other_cpu_locked(t->effective_priority);
}

void sched_ready(tcb_t *t) {
    uint64_t flags = spin_lock_irqsave(&g_sched_lock);
    sched_ready_locked(t);
    spin_unlock_irqrestore(&g_sched_lock, flags);
}

void sched_unready(tcb_t *t) {
    uint64_t flags = spin_lock_irqsave(&g_sched_lock);
    ready_remove_locked(t);
    spin_unlock_irqrestore(&g_sched_lock, flags);
}

tcb_t *sched_current(void) {
    cpu_local_t *cpu = smp_current_cpu();
    return cpu ? cpu->current : NULL;
}

void sched_activate(void) {
    __atomic_store_n(&g_sched_active, 1, __ATOMIC_RELEASE);
    uint32_t total = smp_cpu_count();
    for (uint32_t i = 1u; i < total; i++) {
        arch_smp_send_resched_ipi(i);
    }
}

void sched_block_current(thread_state_t state) {
    uint64_t flags = spin_lock_irqsave(&g_sched_lock);
    tcb_t *t = sched_current();
    if (!t || t->is_idle) {
        spin_unlock_irqrestore(&g_sched_lock, flags);
        panic("sched: block without current user thread");
    }
    t->state = state;
    schedule_with_lock(flags);
}

void sched_unblock_locked(tcb_t *t) {
    if (!t || t->is_idle || t->state == THREAD_RUNNING ||
        t->state == THREAD_READY || t->state == THREAD_ZOMBIE ||
        t->state == THREAD_EMBRYO) {
        return;
    }
    sched_ready_locked(t);
}

int sched_unblock_nolock_preempt(tcb_t *t) {
    uint64_t flags = spin_lock_irqsave(&g_sched_lock);
    int need_preempt = 0;
    if (t && !t->is_idle && t->state != THREAD_RUNNING &&
        t->state != THREAD_READY && t->state != THREAD_ZOMBIE &&
        t->state != THREAD_EMBRYO) {
        sched_ready_locked(t);
        tcb_t *cur = sched_current();
        if (cur && !cur->is_idle && cur->state == THREAD_RUNNING &&
            t->effective_priority > cur->effective_priority) {
            need_preempt = 1;
        }
    }
    spin_unlock_irqrestore(&g_sched_lock, flags);
    return need_preempt;
}

void sched_unblock(tcb_t *t) {
    if (sched_unblock_nolock_preempt(t)) {
        schedule();
    }
}

/* switch.S accesses the TCB at fixed offsets; fail the build if they drift. */
_Static_assert(offsetof(tcb_t, kernel_rsp) == 32, "switch.S TCB_KRSP mismatch");
_Static_assert(offsetof(tcb_t, fpu_area) == 40, "switch.S TCB_FPU mismatch");

void sched_init_cpu_idle(cpu_local_t *cpu) {
    tcb_t *idle = kzalloc(sizeof(tcb_t));
    uint8_t *fpu_raw = kmalloc(g_fpu_size + g_fpu_align);
    if (!idle || !fpu_raw) {
        panic("sched: out of memory for per-CPU idle TCB");
    }
    uintptr_t mask = (uintptr_t)(g_fpu_align - 1u);
    uint8_t *fpu = (uint8_t *)(((uintptr_t)fpu_raw + mask) & ~mask);
    memcpy(fpu, g_fpu_template, g_fpu_size);

    idle->tid = 0u;
    idle->pid = 0u;
    idle->state = THREAD_RUNNING;
    idle->is_idle = 1u;
    idle->on_cpu = 1u;
    idle->proc = NULL;
    idle->fpu_area = fpu;
    idle->fpu_raw = fpu_raw;

    cpu->idle_tcb = idle;
    cpu->current = idle;
    cpu->is_idle = 1u;
    arch_fpu_restore_current(fpu);
}

void sched_init(void) {
    g_fpu_size = arch_fpu_state_size();
    g_fpu_align = arch_fpu_state_align();
    if (g_fpu_align < 16u) {
        g_fpu_align = 16u;
    }
    uint8_t *raw = kmalloc(g_fpu_size + g_fpu_align);
    if (!raw) {
        panic("sched: out of memory for XSAVE template");
    }
    uintptr_t mask = (uintptr_t)(g_fpu_align - 1u);
    g_fpu_template = (uint8_t *)(((uintptr_t)raw + mask) & ~mask);
    arch_fpu_init_template(g_fpu_template);
    sched_init_cpu_idle(smp_current_cpu());
    printk("[sched] 256-level SMP bitmap scheduler ready (fpu=%llu align=%llu)"
           "\n",
           (unsigned long long)g_fpu_size, (unsigned long long)g_fpu_align);
}

void sched_reset_current_fpu(void) {
    tcb_t *t = sched_current();
    if (!t || !t->fpu_area || !g_fpu_template) {
        return;
    }
    memcpy(t->fpu_area, g_fpu_template, g_fpu_size);
    arch_fpu_restore_current(t->fpu_area);
}

static void apply_switch(cpu_local_t *cpu, tcb_t *next) {
    uint64_t target_cr3 =
        next->proc ? next->proc->pml4_phys : vmm_kernel_pml4();
    if (vmm_get_cr3() != target_cr3) {
        vmm_set_cr3(target_cr3);
    }
    if (next->proc) {
        arch_set_kernel_stack(next->kstack_top);
        cpu->syscall_kstack = next->kstack_top;
        arch_set_tls_base(next->fs_base);
        __atomic_store_n(&cpu->is_idle, 0u, __ATOMIC_RELEASE);
    } else {
        __atomic_store_n(&cpu->is_idle, 1u, __ATOMIC_RELEASE);
    }
    next->state = THREAD_RUNNING;
    next->slice_ticks = 0;
    cpu->current = next;
}

void sched_post_switch(void) {
    cpu_local_t *cpu = smp_current_cpu();
    tcb_t *prev = cpu->prev_switch;
    cpu->prev_switch = NULL;
    if (prev) {
        if (!prev->is_idle && prev->state == THREAD_READY) {
            sched_ready_locked(prev);
        }
        __atomic_store_n(&prev->on_cpu, 0u, __ATOMIC_RELEASE);
    }
    spin_unlock(&g_sched_lock);
}

void schedule_with_lock(uint64_t saved_flags) {
    cpu_local_t *cpu = smp_current_cpu();
    tcb_t *prev = cpu->current;
    if (!prev) {
        spin_unlock_irqrestore(&g_sched_lock, saved_flags);
        return;
    }

    if (!prev->is_idle &&
        __atomic_load_n(&prev->kill_pending, __ATOMIC_ACQUIRE)) {
        prev->state = THREAD_ZOMBIE;
    }

    tcb_t *next = pick_next_locked();

    if (prev->is_idle) {
        if (!next) {
            spin_unlock_irqrestore(&g_sched_lock, saved_flags);
            return;
        }
    } else if (prev->state == THREAD_RUNNING) {
        if (!next || next->effective_priority < prev->effective_priority) {
            prev->slice_ticks = 0;
            spin_unlock_irqrestore(&g_sched_lock, saved_flags);
            return;
        }
        /* Mark READY; sched_post_switch() enqueues after leaving prev stack. */
        prev->state = THREAD_READY;
    } else {
        /* prev is blocked or ZOMBIE: switch to next runnable or idle_tcb. */
        if (!next) {
            next = cpu->idle_tcb;
        }
    }

    if (next != cpu->idle_tcb) {
        ready_remove_locked(next);
    }
    if (next == prev) {
        prev->state = THREAD_RUNNING;
        prev->slice_ticks = 0;
        spin_unlock_irqrestore(&g_sched_lock, saved_flags);
        return;
    }

    __atomic_store_n(&next->on_cpu, 1u, __ATOMIC_RELEASE);
    cpu->prev_switch = prev;
    apply_switch(cpu, next);

    switch_to(prev, next);
    sched_post_switch();
    arch_irq_restore(saved_flags);
}

void schedule(void) {
    uint64_t flags = spin_lock_irqsave(&g_sched_lock);
    schedule_with_lock(flags);
}

void sched_idle_loop(void) {
    for (;;) {
        schedule();
        arch_cpu_idle_wait();
    }
}

void sched_ap_enter_idle(void) {
    while (!__atomic_load_n(&g_sched_active, __ATOMIC_ACQUIRE)) {
        arch_cpu_relax();
    }
    sched_idle_loop();
}

void sched_tick(int from_user) {
    if (!__atomic_load_n(&g_sched_active, __ATOMIC_ACQUIRE)) {
        return;
    }
    if (!from_user) {
        return;
    }
    tcb_t *cur = sched_current();
    if (!cur || cur->is_idle || cur->state != THREAD_RUNNING) {
        return;
    }
    if (cur->proc) {
        __atomic_fetch_add(&cur->proc->utime_ticks, 1ULL, __ATOMIC_RELAXED);
    }
    if (__atomic_load_n(&cur->kill_pending, __ATOMIC_ACQUIRE)) {
        schedule();
        return;
    }
    cur->slice_ticks++;
    if (cur->slice_ticks >= SLICE_TICKS) {
        uint64_t flags = spin_lock_irqsave(&g_sched_lock);
        if (!ready_empty_locked()) {
            schedule_with_lock(flags);
            return;
        }
        cur->slice_ticks = 0;
        spin_unlock_irqrestore(&g_sched_lock, flags);
    }
}

void sched_resched_ipi(int from_user) {
    if (!__atomic_load_n(&g_sched_active, __ATOMIC_ACQUIRE)) {
        return;
    }
    if (!from_user) {
        return;
    }
    schedule();
}

long sys_sched_yield(void) {
    uint64_t flags = spin_lock_irqsave(&g_sched_lock);
    tcb_t *cur = sched_current();
    if (!cur || cur->is_idle || cur->state != THREAD_RUNNING) {
        spin_unlock_irqrestore(&g_sched_lock, flags);
        return 0;
    }
    if (!ready_empty_locked()) {
        schedule_with_lock(flags);
        return 0;
    }
    spin_unlock_irqrestore(&g_sched_lock, flags);
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
        .utime = (int64_t)__atomic_load_n(&p->utime_ticks, __ATOMIC_RELAXED),
        .stime = (int64_t)__atomic_load_n(&p->stime_ticks, __ATOMIC_RELAXED),
        .cutime = (int64_t)__atomic_load_n(&p->cutime_ticks, __ATOMIC_RELAXED),
        .cstime = (int64_t)__atomic_load_n(&p->cstime_ticks, __ATOMIC_RELAXED),
    };
    if (copy_to_user(ubuf, &kt, sizeof(kt)) < 0) {
        return -EFAULT;
    }
    return (long)(time_monotonic_ns() / 1000000ULL);
}

/* ---- processes ---- */

static pcb_t *g_proc_list;
static uint32_t g_next_pid = 1u;

uint32_t proc_alloc_pid(void) {
    return __atomic_fetch_add(&g_next_pid, 1u, __ATOMIC_RELAXED);
}

pcb_t *proc_find_locked(uint32_t pid) {
    for (pcb_t *p = g_proc_list; p; p = p->next) {
        if (p->pid == pid && p->state != PROC_DEAD) {
            return p;
        }
    }
    return NULL;
}

pcb_t *proc_find(uint32_t pid) {
    uint64_t flags = spin_lock_irqsave(&g_proc_lock);
    pcb_t *p = proc_find_locked(pid);
    spin_unlock_irqrestore(&g_proc_lock, flags);
    return p;
}

pcb_t *proc_current(void) {
    tcb_t *cur = sched_current();
    return (cur && cur->proc) ? cur->proc : NULL;
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
    p->pgid = (parent && parent->pgid) ? parent->pgid : p->pid;
    p->sid = (parent && parent->sid) ? parent->sid : p->pid;
    p->has_execed = 0;
    p->state = PROC_ALIVE;
    p->pml4_phys = vmm_create_user_pml4();
    if (!p->pml4_phys) {
        kfree(p);
        return NULL;
    }
    p->conn_table = kzalloc(CONN_TABLE_SIZE * sizeof(*p->conn_table));
    if (!p->conn_table) {
        vmm_destroy_user_pml4(p->pml4_phys);
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
    if (parent && parent->cwd[0] == '/') {
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        memcpy(p->cwd, parent->cwd, sizeof(p->cwd));
        p->umask = parent->umask;
        p->uid = parent->uid;
        p->euid = parent->euid;
        p->suid = parent->suid;
        p->gid = parent->gid;
        p->egid = parent->egid;
        p->sgid = parent->sgid;
        p->ngroups = parent->ngroups;
        memcpy(p->groups, parent->groups, sizeof(p->groups));
        p->nice = parent->nice;
        memcpy(p->rlimits, parent->rlimits, sizeof(p->rlimits));
        p->next = g_proc_list;
        g_proc_list = p;
        spin_unlock_irqrestore(&g_proc_lock, pf);
    } else {
        p->cwd[0] = '/';
        p->cwd[1] = '\0';
        p->umask = 0022u;
        p->uid = 0u;
        p->euid = 0u;
        p->suid = 0u;
        p->gid = 0u;
        p->egid = 0u;
        p->sgid = 0u;
        p->ngroups = 1u;
        p->groups[0] = 0u;
        p->nice = 0;
        for (uint32_t i = 0; i < PROC_RLIMIT_COUNT; i++) {
            p->rlimits[i].rlim_cur = PROC_RLIM_INFINITY;
            p->rlimits[i].rlim_max = PROC_RLIM_INFINITY;
        }
        p->rlimits[0].rlim_cur = 0ULL;         /* RLIMIT_CORE */
        p->rlimits[4].rlim_cur = PROC_MAX_FDS; /* RLIMIT_NOFILE */
        p->rlimits[4].rlim_max = PROC_MAX_FDS;
        p->rlimits[5].rlim_cur = 65536ULL; /* RLIMIT_STACK */
        p->rlimits[5].rlim_max = 65536ULL;
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        p->next = g_proc_list;
        g_proc_list = p;
        spin_unlock_irqrestore(&g_proc_lock, pf);
    }
    return p;
}

tcb_t *thread_create_embryo(pcb_t *p, uint8_t priority) {
    tcb_t *t = kzalloc(sizeof(tcb_t));
    uint8_t *kstack = kmalloc(KERNEL_STACK_SIZE);
    uint8_t *fpu_raw = kmalloc(g_fpu_size + g_fpu_align);
    if (!t || !kstack || !fpu_raw) {
        panic("thread_create: out of kernel memory");
    }
    uintptr_t mask = (uintptr_t)(g_fpu_align - 1u);
    uint8_t *fpu = (uint8_t *)(((uintptr_t)fpu_raw + mask) & ~mask);
    memcpy(fpu, g_fpu_template, g_fpu_size);

    t->tid = __atomic_fetch_add(&g_next_tid, 1u, __ATOMIC_RELAXED);
    t->pid = p->pid;
    t->state = THREAD_EMBRYO;
    t->base_priority = priority;
    t->effective_priority = priority;
    t->proc = p;
    t->kstack_top = (uint64_t)kstack + KERNEL_STACK_SIZE;
    t->fpu_area = fpu;
    t->fpu_raw = fpu_raw;

    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    t->proc_next = p->threads;
    p->threads = t;
    p->nthreads++;
    spin_unlock_irqrestore(&g_proc_lock, pf);
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
    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    tcb_t *found = NULL;
    for (tcb_t *t = p->threads; t; t = t->proc_next) {
        if (t->tid == tid && t->state != THREAD_ZOMBIE) {
            found = t;
            break;
        }
    }
    spin_unlock_irqrestore(&g_proc_lock, pf);
    return found;
}

static void stop_remote_thread(tcb_t *t) {
    __atomic_store_n(&t->kill_pending, 1u, __ATOMIC_RELEASE);
    sched_unready(t);
    ipc_detach_thread(t);

    /* If running on another core, nudge all other cores and wait until off-CPU.
     */
    while (__atomic_load_n(&t->on_cpu, __ATOMIC_ACQUIRE) != 0u) {
        uint32_t total = smp_cpu_count();
        uint32_t self = arch_cpu_index();
        for (uint32_t i = 0; i < total; i++) {
            if (i != self) {
                arch_smp_send_resched_ipi(i);
            }
        }
        arch_cpu_relax();
    }

    uint64_t sf = spin_lock_irqsave(&g_sched_lock);
    ready_remove_locked(t);
    t->state = THREAD_ZOMBIE;
    spin_unlock_irqrestore(&g_sched_lock, sf);
}

void thread_exit_current(int status) {
    tcb_t *t = sched_current();
    pcb_t *p = t->proc;

    ipc_detach_thread(t);

    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    int last_thread = 0;
    if (p->nthreads > 0u) {
        p->nthreads--;
    }
    if (p->nthreads == 0u) {
        last_thread = 1;
    }
    spin_unlock_irqrestore(&g_proc_lock, pf);

    if (last_thread) {
        cov_dump_proc(p);
        time_cleanup_proc(p->pid);
        proc_close_fds(p, 0);

        pf = spin_lock_irqsave(&g_proc_lock);
        p->state = PROC_ZOMBIE;
        p->exit_code = status;
        pcb_t *parent = p->parent_pid ? proc_find_locked(p->parent_pid) : NULL;
        if (parent) {
            uint64_t sf = spin_lock_irqsave(&g_sched_lock);
            for (tcb_t *wt = parent->threads; wt; wt = wt->proc_next) {
                if (wt->state == THREAD_WAIT_BLOCKED) {
                    sched_ready_locked(wt);
                }
            }
            spin_unlock_irqrestore(&g_sched_lock, sf);
        }
        spin_unlock_irqrestore(&g_proc_lock, pf);
        printk("[proc] pid %u became zombie (exit %d)\n", p->pid, status);
    }

    uint64_t sf = spin_lock_irqsave(&g_sched_lock);
    t->state = THREAD_ZOMBIE;
    schedule_with_lock(sf);
    panic("thread_exit_current: returned to zombie");
}

void proc_kill_other_threads(pcb_t *p, tcb_t *keep) {
    for (;;) {
        tcb_t *victim = NULL;
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        for (tcb_t *x = p->threads; x; x = x->proc_next) {
            if (x != keep && x->state != THREAD_ZOMBIE) {
                victim = x;
                break;
            }
        }
        spin_unlock_irqrestore(&g_proc_lock, pf);
        if (!victim) {
            break;
        }
        stop_remote_thread(victim);
    }
    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    p->nthreads = 1u;
    spin_unlock_irqrestore(&g_proc_lock, pf);
}

void proc_reap_zombie_threads(pcb_t *p) {
    if (!p) {
        return;
    }
    tcb_t *cur = sched_current();
    tcb_t *reaped_head = NULL;

    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    tcb_t **pp = &p->threads;
    while (*pp) {
        tcb_t *t = *pp;
        if (t != cur && t->state == THREAD_ZOMBIE &&
            __atomic_load_n(&t->on_cpu, __ATOMIC_ACQUIRE) == 0u) {
            *pp = t->proc_next;
            t->proc_next = reaped_head;
            reaped_head = t;
            continue;
        }
        pp = &t->proc_next;
    }
    spin_unlock_irqrestore(&g_proc_lock, pf);

    while (reaped_head) {
        tcb_t *t = reaped_head;
        reaped_head = t->proc_next;
        if (t->user_stack_base && p->pml4_phys) {
            uint64_t freed[16];
            size_t nfreed = 0u;
            for (uint64_t off = 0; off < 0x10000ULL; off += 4096ULL) {
                uint64_t phys =
                    vmm_unmap_page_to(p->pml4_phys, t->user_stack_base + off);
                if (phys && nfreed < 16u) {
                    freed[nfreed++] = phys;
                }
            }
            if (nfreed > 0u) {
                vmm_tlb_shootdown(p->pml4_phys, 0ULL);
                for (size_t i = 0; i < nfreed; i++) {
                    pmm_free_frame(freed[i]);
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
    }
}

void proc_reap_zombie(pcb_t *zombie) {
    if (!zombie) {
        return;
    }
    /* Wait until the zombie's final thread has switched off its CPU stack. */
    for (tcb_t *t = zombie->threads; t; t = t->proc_next) {
        while (__atomic_load_n(&t->on_cpu, __ATOMIC_ACQUIRE) != 0u) {
            arch_cpu_relax();
        }
    }

    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    pcb_t **pp = &g_proc_list;
    while (*pp) {
        if (*pp == zombie) {
            *pp = zombie->next;
            break;
        }
        pp = &(*pp)->next;
    }
    spin_unlock_irqrestore(&g_proc_lock, pf);

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
    kfree(zombie);
}

void proc_exit_current(int status) {
    tcb_t *t = sched_current();
    pcb_t *p = t->proc;
    proc_kill_other_threads(p, t);
    thread_exit_current(status);
}

void proc_exit_signal(int sig) {
    tcb_t *t = sched_current();
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
        uint8_t has_timeout = 0u;
        uint64_t deadline = 0ULL;
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
            deadline =
                (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
            if (time_clock_now_ns((int)clock_id) >= deadline) {
                return -ETIMEDOUT;
            }
            has_timeout = 1u;
        }

        /* Hold g_futex_lock while verifying *uaddr == val and transitioning
         * into THREAD_FUTEX_BLOCKED under g_sched_lock to prevent lost wakeups
         * against concurrent FUTEX_WAKE on another core. */
        uint64_t ff = spin_lock_irqsave(&g_futex_lock);
        uint32_t cur_val = 0;
        if (copy_from_user(&cur_val, uaddr, sizeof(cur_val)) < 0) {
            spin_unlock_irqrestore(&g_futex_lock, ff);
            return -EFAULT;
        }
        if (cur_val != (uint32_t)val) {
            spin_unlock_irqrestore(&g_futex_lock, ff);
            return -EAGAIN;
        }
        spin_lock(&g_sched_lock);
        cur->futex_has_timeout = has_timeout;
        cur->sleep_clock_id = (uint8_t)clock_id;
        cur->sleep_deadline_ns = deadline;
        cur->futex_addr = uaddr;
        cur->futex_woken = 0;
        cur->state = THREAD_FUTEX_BLOCKED;
        spin_unlock(&g_futex_lock);
        schedule_with_lock(ff);

        cur->futex_addr = 0;
        cur->futex_has_timeout = 0;
        if (cur->futex_woken) {
            return 0;
        }
        if (__atomic_load_n(&cur->pending_signals, __ATOMIC_ACQUIRE) &
            ~cur->blocked_signals) {
            return -EINTR;
        }
        return -ETIMEDOUT;
    }

    if (op == FUTEX_WAKE) {
        uint64_t woken = 0;
        int need_preempt = 0;
        uint64_t pf = spin_lock_irqsave(&g_proc_lock);
        spin_lock(&g_futex_lock);
        spin_lock(&g_sched_lock);
        for (tcb_t *t = p->threads; t && woken < val; t = t->proc_next) {
            if (t->state == THREAD_FUTEX_BLOCKED && t->futex_addr == uaddr) {
                t->futex_addr = 0;
                t->futex_has_timeout = 0;
                t->futex_woken = 1;
                sched_ready_locked(t);
                if (t->effective_priority > cur->effective_priority) {
                    need_preempt = 1;
                }
                woken++;
            }
        }
        spin_unlock(&g_sched_lock);
        spin_unlock(&g_futex_lock);
        spin_unlock_irqrestore(&g_proc_lock, pf);
        if (need_preempt) {
            schedule();
        }
        return (long)woken;
    }

    return -EINVAL;
}
