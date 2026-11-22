/**
 * @file     sysconf.c
 * @brief    POSIX.1-2024 sysconf, confstr, pathconf, fpathconf, gethostname
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* need:: COMP_PROC_0009 */

long sysconf(int name) {
    switch (name) {
    case _SC_ARG_MAX:
        return ARG_MAX;
    case _SC_CHILD_MAX:
        return _POSIX_CHILD_MAX;
    case _SC_CLK_TCK:
        return 1000L;
    case _SC_NGROUPS_MAX:
        return _POSIX_NGROUPS_MAX;
    case _SC_OPEN_MAX:
        return OPEN_MAX;
    case _SC_STREAM_MAX:
        return 32L;
    case _SC_TZNAME_MAX:
        return _POSIX_TZNAME_MAX;
    case _SC_JOB_CONTROL:
        return 1L;
    case _SC_SAVED_IDS:
        return 1L;
    case _SC_VERSION:
        return _POSIX_VERSION;
    case _SC_PAGESIZE:
        return 4096L;
    case _SC_2_VERSION:
        return _POSIX2_VERSION;
    case _SC_2_C_BIND:
        return _POSIX2_VERSION;
    case _SC_2_C_DEV:
    case _SC_2_CHAR_TERM:
    case _SC_2_LOCALEDEF:
    case _SC_2_SW_DEV:
    case _SC_2_UPE:
        return -1L;
    case _SC_BC_BASE_MAX:
        return 99L;
    case _SC_BC_DIM_MAX:
        return 2048L;
    case _SC_BC_SCALE_MAX:
        return 99L;
    case _SC_BC_STRING_MAX:
        return 1000L;
    case _SC_COLL_WEIGHTS_MAX:
        return 2L;
    case _SC_EXPR_NEST_MAX:
        return 32L;
    case _SC_LINE_MAX:
        return LINE_MAX;
    case _SC_RE_DUP_MAX:
        return _POSIX_RE_DUP_MAX;
    case _SC_ASYNCHRONOUS_IO:
        return _POSIX_VERSION;
    case _SC_FSYNC:
        return -1L;
    case _SC_MAPPED_FILES:
        return _POSIX_VERSION;
    case _SC_MEMLOCK:
        return _POSIX_MEMLOCK;
    case _SC_MEMLOCK_RANGE:
        return _POSIX_MEMLOCK_RANGE;
    case _SC_MEMORY_PROTECTION:
        return _POSIX_VERSION;
    case _SC_MESSAGE_PASSING:
    case _SC_PRIORITIZED_IO:
    case _SC_PRIORITY_SCHEDULING:
    case _SC_REALTIME_SIGNALS:
    case _SC_SYNCHRONIZED_IO:
    case _SC_SPAWN:
        return -1L;
    case _SC_SEMAPHORES:
        return _POSIX_VERSION;
    case _SC_SHARED_MEMORY_OBJECTS:
        return _POSIX_SHARED_MEMORY_OBJECTS;
    case _SC_TIMERS:
    case _SC_THREADS:
    case _SC_THREAD_SAFE_FUNCTIONS:
    case _SC_MONOTONIC_CLOCK:
    case _SC_CLOCK_SELECTION:
    case _SC_BARRIERS:
    case _SC_READER_WRITER_LOCKS:
    case _SC_SPIN_LOCKS:
        return _POSIX_VERSION;
    case _SC_THREAD_DESTRUCTOR_ITERATIONS:
        return 4L;
    case _SC_THREAD_KEYS_MAX:
        return 64L;
    case _SC_THREAD_STACK_MIN:
        return 16384L;
    case _SC_THREAD_THREADS_MAX:
        return 64L;
    case _SC_HOST_NAME_MAX:
        return _POSIX_HOST_NAME_MAX;
    case _SC_LOGIN_NAME_MAX:
        return _POSIX_LOGIN_NAME_MAX;
    case _SC_TTY_NAME_MAX:
        return _POSIX_TTY_NAME_MAX;
    case _SC_SYMLOOP_MAX:
        return _POSIX_SYMLOOP_MAX;
    case _SC_IOV_MAX:
        return IOV_MAX;
    case _SC_ATEXIT_MAX:
        return 32L;
    case _SC_GETGR_R_SIZE_MAX:
    case _SC_GETPW_R_SIZE_MAX:
        return 1024L;
    case _SC_V8_LP64_OFF64:
        return 1L;
    default:
        errno = EINVAL;
        return -1L;
    }
}

size_t confstr(int name, char *buf, size_t len) {
    const char *val = NULL;
    switch (name) {
    case _CS_PATH:
        val = "/boot";
        break;
    case _CS_POSIX_V8_LP64_OFF64_CFLAGS:
    case _CS_POSIX_V8_LP64_OFF64_LDFLAGS:
    case _CS_POSIX_V8_LP64_OFF64_LIBS:
    case _CS_V8_ENV:
        val = "";
        break;
    case _CS_POSIX_V8_WIDTH_RESTRICTED_ENVS:
        val = "POSIX_V8_LP64_OFF64";
        break;
    default:
        errno = EINVAL;
        return 0u;
    }

    size_t need = strlen(val) + 1u;
    if (buf != NULL && len > 0u) {
        size_t copy_n = (need - 1u < len - 1u) ? (need - 1u) : (len - 1u);
        if (copy_n > 0u) {
            memcpy(buf, val, copy_n);
        }
        buf[copy_n] = '\0';
    }
    return need;
}

static long pathconf_value(int name) {
    switch (name) {
    case _PC_LINK_MAX:
        return _POSIX_LINK_MAX;
    case _PC_MAX_CANON:
        return MAX_CANON;
    case _PC_MAX_INPUT:
        return MAX_INPUT;
    case _PC_NAME_MAX:
        return NAME_MAX;
    case _PC_PATH_MAX:
        return PATH_MAX;
    case _PC_PIPE_BUF:
        return PIPE_BUF;
    case _PC_CHOWN_RESTRICTED:
        return 1L;
    case _PC_NO_TRUNC:
        return 1L;
    case _PC_VDISABLE:
        return 0L;
    case _PC_ASYNC_IO:
        return 1L;
    case _PC_PRIO_IO:
    case _PC_SYNC_IO:
        return -1L;
    case _PC_FILESIZEBITS:
        return 32L;
    case _PC_SYMLINK_MAX:
        return _POSIX_SYMLINK_MAX;
    case _PC_2_SYMLINKS:
        return -1L;
    default:
        errno = EINVAL;
        return -1L;
    }
}

long pathconf(const char *path, int name) {
    if (!path) {
        errno = EFAULT;
        return -1L;
    }
    struct stat st;
    if (stat(path, &st) != 0) {
        return -1L;
    }
    return pathconf_value(name);
}

long fpathconf(int fildes, int name) {
    struct stat st;
    if (fstat(fildes, &st) != 0) {
        return -1L;
    }
    return pathconf_value(name);
}

int gethostname(char *name, size_t namelen) {
    if (!name) {
        errno = EFAULT;
        return -1;
    }
    static const char k_host[] = "microkernel";
    if (namelen > 0u) {
        size_t copy_n = sizeof(k_host) - 1u;
        if (copy_n >= namelen) {
            copy_n = namelen - 1u;
        }
        if (copy_n > 0u) {
            memcpy(name, k_host, copy_n);
        }
        name[copy_n] = '\0';
    }
    return 0;
}
