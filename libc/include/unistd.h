// Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
// SPDX-License-Identifier: MIT

#pragma once
#include <stddef.h>
#include <sys/types.h>
#include <sys/microkernel.h>

extern char **environ;
extern void *__auxv;

ssize_t write(int fd, const void *buf, size_t n);
ssize_t read(int fd, void *buf, size_t n);
void _exit(int status) __attribute__((noreturn));
unsigned int sleep(unsigned int seconds);
