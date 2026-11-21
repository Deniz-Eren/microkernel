/**
 * @file     fd_internal.h
 * @brief    Internal libc helpers bridging unistd and Ring 3 /dev/pipe IPC
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stddef.h>
#include <sys/types.h>

ssize_t __libc_fd_read(int fd, void *buf, size_t n);
ssize_t __libc_fd_write(int fd, const void *buf, size_t n);
int __libc_fd_prefork(void);
void __libc_fd_postfork_rollback(void);
