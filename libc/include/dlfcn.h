/**
 * @file     dlfcn.h
 * @brief    POSIX.1-2024 dynamic linking interfaces (dlopen/dlsym/dladdr)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _DLFCN_H
#define _DLFCN_H

/* need:: ICD_LIBC_0016 */

#define RTLD_LAZY 0x0001
#define RTLD_NOW 0x0002
#define RTLD_GLOBAL 0x0100
#define RTLD_LOCAL 0x0000

#define RTLD_NEXT ((void *)-1)
#define RTLD_DEFAULT ((void *)0)

typedef struct {
    const char *dli_fname;
    void *dli_fbase;
    const char *dli_sname;
    void *dli_saddr;
} Dl_info_t;

int dladdr(const void *restrict addr, Dl_info_t *restrict dlip);
int dlclose(void *handle);
char *dlerror(void);
void *dlopen(const char *file, int mode);
void *dlsym(void *restrict handle, const char *restrict name);

#endif /* _DLFCN_H */
