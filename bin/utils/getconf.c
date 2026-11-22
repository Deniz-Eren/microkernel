/**
 * @file     getconf.c
 * @brief    POSIX.1-2024 XCU getconf utility (get configuration values)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* need:: COMP_PROC_0009 */

typedef struct {
    const char *name;
    int key;
} conf_entry_t;

static const conf_entry_t k_cs_vars[] = {
    {"PATH", _CS_PATH},
    {"POSIX_V8_LP64_OFF64_CFLAGS", _CS_POSIX_V8_LP64_OFF64_CFLAGS},
    {"POSIX_V8_LP64_OFF64_LDFLAGS", _CS_POSIX_V8_LP64_OFF64_LDFLAGS},
    {"POSIX_V8_LP64_OFF64_LIBS", _CS_POSIX_V8_LP64_OFF64_LIBS},
    {"POSIX_V8_WIDTH_RESTRICTED_ENVS", _CS_POSIX_V8_WIDTH_RESTRICTED_ENVS},
    {"V8_ENV", _CS_V8_ENV},
};

static const conf_entry_t k_sc_vars[] = {
    {"ARG_MAX", _SC_ARG_MAX},
    {"CHILD_MAX", _SC_CHILD_MAX},
    {"CLK_TCK", _SC_CLK_TCK},
    {"NGROUPS_MAX", _SC_NGROUPS_MAX},
    {"OPEN_MAX", _SC_OPEN_MAX},
    {"STREAM_MAX", _SC_STREAM_MAX},
    {"TZNAME_MAX", _SC_TZNAME_MAX},
    {"_POSIX_JOB_CONTROL", _SC_JOB_CONTROL},
    {"POSIX_JOB_CONTROL", _SC_JOB_CONTROL},
    {"_POSIX_SAVED_IDS", _SC_SAVED_IDS},
    {"POSIX_SAVED_IDS", _SC_SAVED_IDS},
    {"_POSIX_VERSION", _SC_VERSION},
    {"POSIX_VERSION", _SC_VERSION},
    {"PAGE_SIZE", _SC_PAGE_SIZE},
    {"PAGESIZE", _SC_PAGESIZE},
    {"_POSIX2_VERSION", _SC_2_VERSION},
    {"POSIX2_VERSION", _SC_2_VERSION},
    {"_POSIX2_C_BIND", _SC_2_C_BIND},
    {"POSIX2_C_BIND", _SC_2_C_BIND},
    {"_POSIX2_C_DEV", _SC_2_C_DEV},
    {"POSIX2_C_DEV", _SC_2_C_DEV},
    {"_POSIX2_CHAR_TERM", _SC_2_CHAR_TERM},
    {"POSIX2_CHAR_TERM", _SC_2_CHAR_TERM},
    {"_POSIX2_LOCALEDEF", _SC_2_LOCALEDEF},
    {"POSIX2_LOCALEDEF", _SC_2_LOCALEDEF},
    {"_POSIX2_SW_DEV", _SC_2_SW_DEV},
    {"POSIX2_SW_DEV", _SC_2_SW_DEV},
    {"_POSIX2_UPE", _SC_2_UPE},
    {"POSIX2_UPE", _SC_2_UPE},
    {"BC_BASE_MAX", _SC_BC_BASE_MAX},
    {"BC_DIM_MAX", _SC_BC_DIM_MAX},
    {"BC_SCALE_MAX", _SC_BC_SCALE_MAX},
    {"BC_STRING_MAX", _SC_BC_STRING_MAX},
    {"COLL_WEIGHTS_MAX", _SC_COLL_WEIGHTS_MAX},
    {"EXPR_NEST_MAX", _SC_EXPR_NEST_MAX},
    {"LINE_MAX", _SC_LINE_MAX},
    {"RE_DUP_MAX", _SC_RE_DUP_MAX},
    {"_POSIX_ASYNCHRONOUS_IO", _SC_ASYNCHRONOUS_IO},
    {"POSIX_ASYNCHRONOUS_IO", _SC_ASYNCHRONOUS_IO},
    {"_POSIX_FSYNC", _SC_FSYNC},
    {"POSIX_FSYNC", _SC_FSYNC},
    {"_POSIX_MAPPED_FILES", _SC_MAPPED_FILES},
    {"POSIX_MAPPED_FILES", _SC_MAPPED_FILES},
    {"_POSIX_MEMLOCK", _SC_MEMLOCK},
    {"POSIX_MEMLOCK", _SC_MEMLOCK},
    {"_POSIX_MEMLOCK_RANGE", _SC_MEMLOCK_RANGE},
    {"POSIX_MEMLOCK_RANGE", _SC_MEMLOCK_RANGE},
    {"_POSIX_MEMORY_PROTECTION", _SC_MEMORY_PROTECTION},
    {"POSIX_MEMORY_PROTECTION", _SC_MEMORY_PROTECTION},
    {"_POSIX_MESSAGE_PASSING", _SC_MESSAGE_PASSING},
    {"POSIX_MESSAGE_PASSING", _SC_MESSAGE_PASSING},
    {"_POSIX_PRIORITIZED_IO", _SC_PRIORITIZED_IO},
    {"POSIX_PRIORITIZED_IO", _SC_PRIORITIZED_IO},
    {"_POSIX_PRIORITY_SCHEDULING", _SC_PRIORITY_SCHEDULING},
    {"POSIX_PRIORITY_SCHEDULING", _SC_PRIORITY_SCHEDULING},
    {"_POSIX_REALTIME_SIGNALS", _SC_REALTIME_SIGNALS},
    {"POSIX_REALTIME_SIGNALS", _SC_REALTIME_SIGNALS},
    {"_POSIX_SEMAPHORES", _SC_SEMAPHORES},
    {"POSIX_SEMAPHORES", _SC_SEMAPHORES},
    {"_POSIX_SHARED_MEMORY_OBJECTS", _SC_SHARED_MEMORY_OBJECTS},
    {"POSIX_SHARED_MEMORY_OBJECTS", _SC_SHARED_MEMORY_OBJECTS},
    {"_POSIX_SYNCHRONIZED_IO", _SC_SYNCHRONIZED_IO},
    {"POSIX_SYNCHRONIZED_IO", _SC_SYNCHRONIZED_IO},
    {"_POSIX_TIMERS", _SC_TIMERS},
    {"POSIX_TIMERS", _SC_TIMERS},
    {"_POSIX_THREADS", _SC_THREADS},
    {"POSIX_THREADS", _SC_THREADS},
    {"_POSIX_THREAD_SAFE_FUNCTIONS", _SC_THREAD_SAFE_FUNCTIONS},
    {"POSIX_THREAD_SAFE_FUNCTIONS", _SC_THREAD_SAFE_FUNCTIONS},
    {"PTHREAD_DESTRUCTOR_ITERATIONS", _SC_THREAD_DESTRUCTOR_ITERATIONS},
    {"PTHREAD_KEYS_MAX", _SC_THREAD_KEYS_MAX},
    {"PTHREAD_STACK_MIN", _SC_THREAD_STACK_MIN},
    {"PTHREAD_THREADS_MAX", _SC_THREAD_THREADS_MAX},
    {"_POSIX_MONOTONIC_CLOCK", _SC_MONOTONIC_CLOCK},
    {"POSIX_MONOTONIC_CLOCK", _SC_MONOTONIC_CLOCK},
    {"_POSIX_CLOCK_SELECTION", _SC_CLOCK_SELECTION},
    {"POSIX_CLOCK_SELECTION", _SC_CLOCK_SELECTION},
    {"_POSIX_BARRIERS", _SC_BARRIERS},
    {"POSIX_BARRIERS", _SC_BARRIERS},
    {"_POSIX_READER_WRITER_LOCKS", _SC_READER_WRITER_LOCKS},
    {"POSIX_READER_WRITER_LOCKS", _SC_READER_WRITER_LOCKS},
    {"_POSIX_SPIN_LOCKS", _SC_SPIN_LOCKS},
    {"POSIX_SPIN_LOCKS", _SC_SPIN_LOCKS},
    {"_POSIX_SPAWN", _SC_SPAWN},
    {"POSIX_SPAWN", _SC_SPAWN},
    {"HOST_NAME_MAX", _SC_HOST_NAME_MAX},
    {"LOGIN_NAME_MAX", _SC_LOGIN_NAME_MAX},
    {"TTY_NAME_MAX", _SC_TTY_NAME_MAX},
    {"SYMLOOP_MAX", _SC_SYMLOOP_MAX},
    {"IOV_MAX", _SC_IOV_MAX},
    {"ATEXIT_MAX", _SC_ATEXIT_MAX},
    {"_POSIX_V8_LP64_OFF64", _SC_V8_LP64_OFF64},
    {"POSIX_V8_LP64_OFF64", _SC_V8_LP64_OFF64},
};

static const conf_entry_t k_pc_vars[] = {
    {"LINK_MAX", _PC_LINK_MAX},
    {"MAX_CANON", _PC_MAX_CANON},
    {"MAX_INPUT", _PC_MAX_INPUT},
    {"NAME_MAX", _PC_NAME_MAX},
    {"PATH_MAX", _PC_PATH_MAX},
    {"PIPE_BUF", _PC_PIPE_BUF},
    {"_POSIX_CHOWN_RESTRICTED", _PC_CHOWN_RESTRICTED},
    {"POSIX_CHOWN_RESTRICTED", _PC_CHOWN_RESTRICTED},
    {"_POSIX_NO_TRUNC", _PC_NO_TRUNC},
    {"POSIX_NO_TRUNC", _PC_NO_TRUNC},
    {"_POSIX_VDISABLE", _PC_VDISABLE},
    {"POSIX_VDISABLE", _PC_VDISABLE},
    {"_POSIX_ASYNC_IO", _PC_ASYNC_IO},
    {"POSIX_ASYNC_IO", _PC_ASYNC_IO},
    {"_POSIX_PRIO_IO", _PC_PRIO_IO},
    {"POSIX_PRIO_IO", _PC_PRIO_IO},
    {"_POSIX_SYNC_IO", _PC_SYNC_IO},
    {"POSIX_SYNC_IO", _PC_SYNC_IO},
    {"FILESIZEBITS", _PC_FILESIZEBITS},
    {"SYMLINK_MAX", _PC_SYMLINK_MAX},
    {"POSIX2_SYMLINKS", _PC_2_SYMLINKS},
};

static int lookup_var(const conf_entry_t *tbl, size_t n, const char *name,
                      int *key_out) {
    for (size_t i = 0; i < n; i++) {
        if (strcmp(tbl[i].name, name) == 0) {
            *key_out = tbl[i].key;
            return 0;
        }
    }
    return -1;
}

int main(int argc, char **argv) {
    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "v:")) != -1) {
        if (c == 'v') {
            if (strcmp(optarg, "POSIX_V8_LP64_OFF64") != 0) {
                (void)fprintf(stderr, "getconf: unsupported specification %s\n",
                              optarg);
                return 1;
            }
        } else {
            (void)fprintf(stderr, "getconf: invalid option -%c\n", optopt);
            return 1;
        }
    }

    int rem = argc - optind;
    if (rem == 1) {
        const char *var = argv[optind];
        int key = 0;
        if (lookup_var(k_cs_vars, sizeof(k_cs_vars) / sizeof(k_cs_vars[0]), var,
                       &key) == 0) {
            char buf[256];
            errno = 0;
            size_t need = confstr(key, buf, sizeof(buf));
            if (need == 0u) {
                if (errno == 0) {
                    (void)puts("undefined");
                    return 0;
                }
                (void)fprintf(stderr, "getconf: confstr failed for %s\n", var);
                return 1;
            }
            (void)puts(buf);
            return 0;
        }
        if (lookup_var(k_sc_vars, sizeof(k_sc_vars) / sizeof(k_sc_vars[0]), var,
                       &key) == 0) {
            errno = 0;
            long val = sysconf(key);
            if (val == -1L && errno == 0) {
                (void)puts("undefined");
                return 0;
            }
            if (val == -1L) {
                (void)fprintf(stderr, "getconf: sysconf failed for %s\n", var);
                return 1;
            }
            (void)printf("%ld\n", val);
            return 0;
        }
        (void)fprintf(stderr, "getconf: unknown variable %s\n", var);
        return 1;
    }

    if (rem == 2) {
        const char *var = argv[optind];
        const char *path = argv[optind + 1];
        int key = 0;
        if (lookup_var(k_pc_vars, sizeof(k_pc_vars) / sizeof(k_pc_vars[0]), var,
                       &key) != 0) {
            (void)fprintf(stderr, "getconf: unknown path variable %s\n", var);
            return 1;
        }
        errno = 0;
        long val = pathconf(path, key);
        if (val == -1L && errno == 0) {
            (void)puts("undefined");
            return 0;
        }
        if (val == -1L) {
            (void)fprintf(stderr, "getconf: pathconf failed for %s on %s\n",
                          var, path);
            return 1;
        }
        (void)printf("%ld\n", val);
        return 0;
    }

    (void)fprintf(stderr,
                  "usage: getconf [-v spec] system_var | path_var pathname\n");
    return 1;
}
