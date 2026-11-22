/**
 * @file     sched.h
 * @brief    Process, thread, and SMP preemptive scheduler interface
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <stddef.h>
#include <stdint.h>
#include <kernel/smp.h>
#include <kernel/spinlock.h>
#include <sys/fd.h>
#include <sys/microkernel.h>
#include <sys/proc.h>

struct trapframe;

typedef enum {
    THREAD_EMBRYO,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_SEND_BLOCKED,
    THREAD_REPLY_BLOCKED,
    THREAD_RECEIVE_BLOCKED,
    THREAD_WAIT_BLOCKED,
    THREAD_SLEEP_BLOCKED,
    THREAD_FUTEX_BLOCKED,
    THREAD_STOPPED,
    THREAD_ZOMBIE,
} thread_state_t;

typedef struct pcb pcb_t;

typedef struct tcb {
    uint32_t tid;
    uint32_t pid;
    thread_state_t state;

    uint8_t base_priority;      /* 0 = lowest, 255 = highest */
    uint8_t effective_priority; /* active priority (inheritance-boosted) */

    pcb_t *proc;
    uint64_t kstack_top; /* top of this thread's 16 KiB kernel stack */
    uint64_t kernel_rsp; /* saved Ring 0 stack pointer (switch.S offset) */
    uint8_t *fpu_area;   /* 64-byte aligned XSAVE extended-state image */

    struct tcb *queue_next; /* ready-queue link */
    struct tcb *proc_next;  /* per-process thread list */

    uint64_t slice_ticks; /* consumed quantum */
    uint32_t pending_signals;
    uint32_t blocked_signals;
    uint32_t sig_wait_set;
    uint32_t saved_sigmask;
    uint8_t in_sigsuspend;
    uint8_t has_saved_sigmask;
    uint8_t futex_is_shared;

    /* IPC state (Milestone 3) */
    void *blocked_on_chan; /* channel_t* this thread is blocked on */
    struct tcb *ipc_next;  /* channel receive-queue link */
    long ipc_result;       /* value handed back when a block resolves */
    uint64_t ipc_buf;      /* user receive buffer (ReceiveMsg arg) */
    uint64_t ipc_buflen;   /* user receive buffer length */
    uint64_t ipc_info;     /* user ipc_recv_info_t* (ReceiveMsg arg) */

    /* Start context (Milestone 4; armed via SYS_THREAD_RESET). */
    uint64_t start_rip;
    uint64_t start_arg;
    uint64_t start_rsp;
    uint8_t iopl;           /* Ring 3 port I/O privilege level (0 or 3) */
    uint8_t sleep_clock_id; /* CLOCK_REALTIME or CLOCK_MONOTONIC */
    uint8_t futex_has_timeout;
    uint8_t futex_woken;
    uint64_t sleep_deadline_ns;
    uint64_t fs_base;         /* x86-64 IA32_FS_BASE per-thread TLS pointer */
    uint64_t futex_addr;      /* user VA this thread is FUTEX_WAIT-blocked on */
    uint8_t *fpu_raw;         /* unaligned kmalloc pointer for fpu_area */
    uint64_t user_stack_base; /* kernel-allocated 64 KiB thread stack base VA */

    /* SMP execution and cross-CPU teardown state. */
    uint8_t is_idle;
    volatile uint8_t on_cpu;
    volatile uint8_t kill_pending;
} tcb_t;

/* switch.S reads these fields at fixed offsets. */
#define TCB_KRSP_OFFSET offsetof(tcb_t, kernel_rsp)
#define TCB_FPU_OFFSET offsetof(tcb_t, fpu_area)

typedef enum { PROC_ALIVE, PROC_ZOMBIE, PROC_DEAD } proc_state_t;

struct pcb {
    uint32_t pid;
    uint32_t parent_pid;
    uint32_t pgid;
    uint32_t sid;
    uint8_t has_execed;
    proc_state_t state;
    int exit_code;
    int exit_signal;
    uint32_t caps; /* privilege capability mask (e.g. CAP_SYS_INTR) */

    uint64_t pml4_phys;
    uint64_t brk;          /* program break (user VA) */
    uint64_t next_mmap_va; /* bump cursor for dynamic VMMap regions */
    uint32_t mlock_flags;  /* MCL_CURRENT / MCL_FUTURE memory lock state */

    tcb_t *threads; /* singly-linked list of live threads */
    uint32_t nthreads;

    /* Connection (connid) table — Milestone 3. */
    struct conn {
        uint32_t pid; /* 0 = free slot */
        uint32_t chanid;
        uint32_t flags;
    } * conn_table;
    uint32_t conn_count;

    /* Signals (Milestone 4 / POSIX.1-2024): per-signal state, 1..31. */
    uint64_t sig_handlers[32];
    uint32_t sig_masks[32];
    uint32_t sig_flags[32];
    uint64_t sig_restorer;  /* __sigreturn_trampoline address */
    uint64_t next_stack_va; /* bump cursor for per-thread user stacks */

    uint64_t sig_values[32];
    uint32_t sig_senders[32];
    uint32_t sig_sender_uids[32];
    uint8_t sig_codes[32];

    uint8_t stopped;
    uint8_t stop_sig;
    uint8_t cont_pending;

    /* Per-process file descriptor table. */
    struct __k_fd_entry fds[PROC_MAX_FDS];

    /* Per-process current working directory, umask, and command metadata. */
    char cwd[PROC_CWD_MAX];
    uint32_t umask;
    char comm[PROC_COMM_MAX];
    char args[PROC_ARGS_MAX];

    /* POSIX.1-2024 process credentials, nice value, and resource limits. */
    uint32_t uid;
    uint32_t euid;
    uint32_t suid;
    uint32_t gid;
    uint32_t egid;
    uint32_t sgid;
    uint32_t ngroups;
    uint32_t groups[PROC_NGROUPS_MAX];
    int32_t nice;
    struct __k_rlimit rlimits[PROC_RLIMIT_COUNT];

    /* Process CPU tick accounting and alarm(2) deadline (monotonic ns). */
    uint64_t utime_ticks;
    uint64_t stime_ticks;
    uint64_t cutime_ticks;
    uint64_t cstime_ticks;
    uint64_t alarm_expire_ns;

#ifdef MICROKERNEL_COVERAGE
    struct __k_cov_desc cov;
    uint8_t cov_registered;
    uint8_t cov_dumped;
#endif

    pcb_t *next; /* global process list */
};

#define KERNEL_STACK_SIZE 16384UL

/* Global process-table and scheduler spinlocks (ordering: g_proc_lock ->
 * g_ipc_lock -> g_sched_lock). */
extern spinlock_t g_proc_lock;
extern spinlock_t g_sched_lock;

void sched_init(void);
void sched_init_cpu_idle(cpu_local_t *cpu);
void sched_ap_enter_idle(void) __attribute__((noreturn));
void sched_idle_loop(void) __attribute__((noreturn));
void sched_post_switch(void);
void sched_tick(int from_user);
void sched_resched_ipi(int from_user);
void schedule(void);
void schedule_with_lock(uint64_t saved_flags);
void sched_ready(tcb_t *t);
void sched_ready_locked(tcb_t *t);
void sched_remove_ready_locked(tcb_t *t);
void sched_unready(tcb_t *t);
void sched_activate(void);
void sched_reset_current_fpu(void);
tcb_t *sched_current(void);

/* Block the current thread on an IPC/other wait queue: marks it non-RUNNING and
 * dispatches the next runnable thread. Returns only when the thread is later
 * made runnable again and picked. */
void sched_block_current(thread_state_t state);

/* Make a blocked thread runnable again; kicks an idle/lower CPU via IPI and
 * returns 1 if the caller should preempt itself once outer locks are dropped.
 */
int sched_unblock_nolock_preempt(tcb_t *t);
void sched_unblock(tcb_t *t);
void sched_unblock_locked(tcb_t *t);

pcb_t *proc_current(void);
pcb_t *proc_find(uint32_t pid);
pcb_t *proc_find_locked(uint32_t pid);
pcb_t *proc_list_head(void);
pcb_t *proc_create(pcb_t *parent);
uint32_t proc_alloc_pid(void);
tcb_t *thread_create(pcb_t *p, uint64_t entry_rip, uint64_t user_rsp,
                     uint8_t priority);

/* Milestone 4 thread lifecycle: create an unarmed EMBRYO thread, then arm
 * it (start context + user stack) to make it runnable. */
tcb_t *thread_create_embryo(pcb_t *p, uint8_t priority);
void thread_arm(tcb_t *t, uint64_t entry_rip, uint64_t arg, uint64_t user_rsp);
void thread_arm_fork(tcb_t *t, const struct trapframe *tf);
tcb_t *thread_find(pcb_t *p, uint32_t tid);
void proc_kill_other_threads(pcb_t *p, tcb_t *keep);
void proc_reap_zombie_threads(pcb_t *p);
void proc_reap_zombie(pcb_t *zombie);
void proc_close_fds(pcb_t *p, int cloexec_only);

/* Allocate a fresh zeroed user stack in p's address space; returns its top
 * VA (stacks grow down), or 0. */
uint64_t proc_user_stack_alloc(pcb_t *p);

/* Milestone 4/5/POSIX thread and process syscalls (caller is current). */
long sys_thread_create(uint64_t prio);
long sys_thread_reset(uint64_t tid, uint64_t entry, uint64_t arg,
                      uint64_t stack_top);
long sys_thread_get_ctx(uint64_t tid, uint64_t ubuf);
long sys_thread_set_fsbase(uint64_t base);
long sys_futex(uint64_t uaddr, uint64_t op, uint64_t val, uint64_t timeout_va,
               uint64_t clock_id);
long sys_proc_create(uint64_t name_va, uint64_t prio, uint64_t flags);
long sys_proc_wait(int64_t pid, uint64_t status_va, uint64_t options);
long sys_proc_fork(const struct trapframe *tf);
long sys_proc_execve(struct trapframe *tf, uint64_t path_va, uint64_t argv_va,
                     uint64_t envp_va);
long sys_proc_getpid(void);
long sys_proc_getppid(void);
long sys_fd_op(uint64_t op, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4);
long sys_sys_stats(uint64_t ubuf);
long sys_sched_yield(void);
long sys_proc_times(uint64_t ubuf);
long sys_proc_pgid(uint64_t op, int64_t pid, int64_t pgid);
long sys_proc_cred(uint64_t op, uint64_t a1, uint64_t a2, uint64_t a3);
long sys_proc_rlimit(uint64_t resource, uint64_t new_va, uint64_t old_va);
void thread_exit_current(int status) __attribute__((noreturn));
void proc_exit_current(int status) __attribute__((noreturn));
void proc_exit_signal(int sig) __attribute__((noreturn));

/* Built by proc_spawn_init(): kernel hands control to sysproc. */
void proc_spawn_init(void);
