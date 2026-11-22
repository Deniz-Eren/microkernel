/**
 * @file     syscall.c
 * @brief    x86-64 syscall dispatcher and SMAP-safe HHDM user copies
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <kernel/arch.h>
#include <kernel/cov.h>
#include <kernel/trapframe.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <kernel/entropy.h>
#include <kernel/intr.h>
#include <kernel/ipc.h>
#include <kernel/signal.h>
#include <kernel/string.h>
#include <kernel/time.h>
#include <kernel/uaccess.h>
#include <kernel/vmm.h>
#include "channel.h"
#include <sys/microkernel.h>
#include <sys/errno.h>

/* need:: COMP_PROC_0002 COMP_MEM_0002 */

/* Copy between an ARBITRARY user address space and the kernel through the HHDM.
 * Each page is translated in the target pml4 and permission-checked; user VAs
 * are never dereferenced directly (SMAP-safe, fault-safe). */
static long copy_user_pml4(uint64_t pml4_phys, void *kbuf, uint64_t uaddr,
                           size_t n, int to_user) {
    size_t done = 0;
    while (done < n) {
        uint64_t page = (uaddr + done) & ~0xFFFULL;
        if (page >= KERNEL_VIRT_BASE && page < 0xFFFF800000000000ULL) {
            return -EFAULT;
        }
        uint64_t pte = vmm_get_pte_to(pml4_phys, page);
        if (!(pte & VMM_PRESENT) || !(pte & VMM_USER)) {
            return -EFAULT;
        }
        if (to_user && !(pte & VMM_WRITE)) {
            if (!(pte & VMM_COW) || vmm_resolve_cow_to(pml4_phys, page) != 0) {
                return -EFAULT; /* read-only target page */
            }
            pte = vmm_get_pte_to(pml4_phys, page);
        }
        uint64_t phys = pte & 0x000FFFFFFFFFF000ULL;
        size_t off = (uaddr + done) & 0xFFF;
        size_t chunk = 4096 - off;
        if (chunk > n - done) {
            chunk = n - done;
        }
        if (to_user) {
            memcpy((uint8_t *)phys_to_virt(phys) + off,
                   (const uint8_t *)kbuf + done, chunk);
        } else {
            memcpy((uint8_t *)kbuf + done,
                   (const uint8_t *)phys_to_virt(phys) + off, chunk);
        }
        done += chunk;
    }
    return (long)n;
}

long copy_from_user_pml4(uint64_t pml4_phys, void *kdst, uint64_t usrc,
                         size_t n) {
    return copy_user_pml4(pml4_phys, kdst, usrc, n, 0);
}

long copy_to_user_pml4(uint64_t pml4_phys, uint64_t udst, const void *ksrc,
                       size_t n) {
    return copy_user_pml4(pml4_phys, (void *)ksrc, udst, n, 1);
}

long copy_from_user(void *kdst, uint64_t usrc, size_t n) {
    return copy_user_pml4(vmm_get_cr3(), kdst, usrc, n, 0);
}

long copy_to_user(uint64_t udst, const void *ksrc, size_t n) {
    return copy_user_pml4(vmm_get_cr3(), (void *)ksrc, udst, n, 1);
}

static long sys_debug_log(uint64_t ubuf, uint64_t len) {
    if (len > 4096) {
        len = 4096;
    }
    static char buf[4096];
    long r = copy_from_user(buf, ubuf, len);
    if (r < 0) {
        return r;
    }
    arch_console_write(buf, (size_t)r);
    return r;
}

void syscall_handler(trapframe_t *tf) {
    uint64_t nr = arch_tf_syscall_num(tf);
    uint64_t a0 = arch_tf_arg0(tf);
    uint64_t a1 = arch_tf_arg1(tf);
    uint64_t a2 = arch_tf_arg2(tf);
    uint64_t a3 = arch_tf_arg3(tf);
    uint64_t a4 = arch_tf_arg4(tf);
    uint64_t a5 = arch_tf_arg5(tf);
    long ret = -ENOSYS;

    /* SIGRETURN restores the interrupted context wholesale; the syscall
     * return value must come from the restored frame, not from `ret`. */
    if (nr == SYS_SIGRETURN) {
        sys_sigreturn(tf);
        return;
    }

    /* EXECVE replaces the address space and rewrites the frame on success. */
    if (nr == SYS_PROC_EXECVE) {
        ret = sys_proc_execve(tf, a0, a1, a2);
        if (ret == 0) {
            return;
        }
        arch_tf_set_ret(tf, ret);
        signal_deliver_check(tf);
        return;
    }

    switch (nr) {
    case SYS_DEBUG_LOG:
        ret = sys_debug_log(a0, a1);
        break;
    case SYS_CHANNEL_CREATE: {
        pcb_t *p = proc_current();
        ret = p ? channel_create(p->pid) : -EPERM;
        break;
    }
    case SYS_CHANNEL_DESTROY: {
        pcb_t *p = proc_current();
        ret = p ? channel_destroy(p->pid, (int)a0) : -EPERM;
        break;
    }
    case SYS_CONNECT_ATTACH:
        ret = ipc_conn_attach((uint32_t)a0, (uint32_t)a1);
        break;
    case SYS_CONNECT_DETACH:
        ret = ipc_conn_detach((int)a0);
        break;
    case SYS_MSG_SENDV:
        ret = ipc_msg_send((int)a0, a1, a2, a3, a4);
        break;
    case SYS_MSG_RECEIVE:
        ret = ipc_msg_receive((int)a0, a1, a2, a3);
        break;
    case SYS_MSG_REPLYV:
        ret = ipc_msg_reply((int)a0, a1, a2);
        break;
    case SYS_MSG_READV:
        ret = ipc_msg_readv((int)a0, a1, a2, a3);
        break;
    case SYS_MSG_WRITEV:
        ret = ipc_msg_writev((int)a0, a1, a2, a3);
        break;
    case SYS_MSG_SEND_PULSE:
        ret = ipc_msg_send_pulse((int)a0, (uint32_t)a1, (uint32_t)a2);
        break;
    case SYS_INTR_ATTACH_EVENT:
        ret = sys_intr_attach_event(a0, a1, a2, a3);
        break;
    case SYS_INTR_UNMASK:
        ret = sys_intr_unmask(a0);
        break;
    case SYS_PROC_CREATE:
        ret = sys_proc_create(a0, a1, a2);
        break;
    case SYS_PROC_EXIT:
        proc_exit_current((int)a0);
        break; /* noreturn */
    case SYS_PROC_WAIT:
        ret = sys_proc_wait((int64_t)a0, a1, a2);
        break;
    case SYS_VM_MAP:
        ret = sys_vm_map(a0, a1, a2, a3);
        break;
    case SYS_VM_UNMAP:
        ret = sys_vm_unmap(a0, a1);
        break;
    case SYS_VM_CLONE_COW:
        ret = sys_vm_clone_cow(a0, a1, a2);
        break;
    case SYS_VM_PROTECT:
        ret = sys_vm_protect(a0, a1, a2);
        break;
    case SYS_CLOCK_GETTIME:
        ret = sys_clock_gettime(a0, a1);
        break;
    case SYS_CLOCK_SETTIME:
        ret = sys_clock_settime(a0, a1);
        break;
    case SYS_CLOCK_GETRES:
        ret = sys_clock_getres(a0, a1);
        break;
    case SYS_CLOCK_NANOSLEEP:
        ret = sys_clock_nanosleep(a0, a1, a2, a3);
        break;
    case SYS_TIMER_CREATE:
        ret = sys_timer_create(a0, a1, a2);
        break;
    case SYS_TIMER_DELETE:
        ret = sys_timer_delete(a0);
        break;
    case SYS_TIMER_SETTIME:
        ret = sys_timer_settime(a0, a1, a2, a3);
        break;
    case SYS_TIMER_GETTIME:
        ret = sys_timer_gettime(a0, a1);
        break;
    case SYS_TIMER_GETOVERRUN:
        ret = sys_timer_getoverrun(a0);
        break;
    case SYS_THREAD_SET_FSBASE:
        ret = sys_thread_set_fsbase(a0);
        break;
    case SYS_THREAD_EXIT:
        thread_exit_current((int)a0);
        break; /* noreturn */
    case SYS_FUTEX:
        ret = sys_futex(a0, a1, a2, a3, a4);
        break;
    case SYS_PROC_FORK:
        ret = sys_proc_fork(tf);
        break;
    case SYS_PROC_GETPID:
        ret = sys_proc_getpid();
        break;
    case SYS_PROC_GETPPID:
        ret = sys_proc_getppid();
        break;
    case SYS_FD_OP:
        ret = sys_fd_op(a0, a1, a2, a3, a4);
        break;
    case SYS_SIGPENDING:
        ret = sys_sigpending(a0);
        break;
    case SYS_GETENTROPY:
        ret = sys_getentropy(a0, a1);
        break;
    case SYS_SHM_OP:
        ret = sys_shm_op(a0, a1, a2, a3);
        break;
    case SYS_SHM_MAP:
        ret = sys_shm_map(a0, a1, a2, a3, a4, a5);
        break;
    case SYS_VM_MLOCK:
        ret = sys_vm_mlock(a0, a1, a2);
        break;
    case SYS_SYS_STATS:
        ret = sys_sys_stats(a0);
        break;
    case SYS_COV_WRITE:
        ret = sys_cov_write(a0, a1);
        break;
    case SYS_SCHED_SET_PRIO: {
        tcb_t *t = sched_current();
        if (a0 > 255) {
            ret = -EINVAL;
        } else {
            t->base_priority = (uint8_t)a0;
            t->effective_priority = (uint8_t)a0;
            ret = 0;
        }
        break;
    }
    case SYS_IOPL_SET:
        ret = sys_iopl_set(tf, a0);
        break;
    case SYS_THREAD_CREATE:
        ret = sys_thread_create(a0);
        break;
    case SYS_THREAD_GET_CTX:
        ret = sys_thread_get_ctx(a0, a1);
        break;
    case SYS_THREAD_RESET:
        ret = sys_thread_reset(a0, a1, a2, a3);
        break;
    case SYS_THREAD_INJECT_SIG:
        ret = sys_inject_sig(a0, a1, a2);
        break;
    case SYS_SIGACTION:
        ret = sys_sigaction(a0, a1, a2, a3);
        break;
    case SYS_SIGPROCMASK:
        ret = sys_sigprocmask(a0, a1, a2);
        break;
    default:
        ret = -ENOSYS;
        break;
    }

    arch_tf_set_ret(tf, ret);

    /* Return-to-user boundary: deliver any pending signals by rewriting the
     * frame (handler runs first; SIGRETURN later restores this context with
     * the syscall result intact). May not return (default action kills). */
    signal_deliver_check(tf);
}
