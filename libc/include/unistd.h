/**
 * @file     unistd.h
 * @brief    POSIX.1-2024 Base unistd declarations and version macros
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _UNISTD_H
#define _UNISTD_H

/* need:: ICD_LIBC_0002 */

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/* POSIX.1-2024 (Issue 8) Base conformance constants. */
#define _POSIX_VERSION 202405L
#define _POSIX2_VERSION 202405L

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

extern char **environ;

ssize_t write(int fd, const void *buf, size_t n);
ssize_t read(int fd, void *buf, size_t n);
_Noreturn void _exit(int status);
unsigned int sleep(unsigned int seconds);

#endif /* _UNISTD_H */
