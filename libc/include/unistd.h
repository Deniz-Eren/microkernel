/**
 * @file     unistd.h
 * @brief    unistd declarations and target POSIX.1-2024 version macros
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _UNISTD_H
#define _UNISTD_H

/* need:: ICD_LIBC_0002 ICD_LIBC_0005 ICD_LIBC_0006 ICD_PROC_0008 */

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/* Target POSIX.1-2024 (IEEE Std 1003.1-2024) version feature macros. */
#define _POSIX_VERSION 202405L
#define _POSIX2_VERSION 202405L
#include <sys/_posix_opts.h>

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

#define GETENTROPY_MAX 256

extern char **environ;
extern char *optarg;
extern int opterr;
extern int optind;
extern int optopt;

unsigned int alarm(unsigned int seconds);
int getopt(int argc, char *const argv[], const char *optstring);
ssize_t write(int fd, const void *buf, size_t n);
ssize_t read(int fd, void *buf, size_t n);
off_t lseek(int fildes, off_t offset, int whence);
int ftruncate(int fildes, off_t length);
int close(int fd);
int dup(int oldfd);
int dup2(int oldfd, int newfd);
int dup3(int oldfd, int newfd, int flags);
int pipe(int fildes[2]);
int pipe2(int fildes[2], int flags);
int unlink(const char *path);
int unlinkat(int fd, const char *path, int flag);
int rmdir(const char *path);
int chdir(const char *path);
int fchdir(int fildes);
char *getcwd(char *buf, size_t size);
int access(const char *path, int amode);
int isatty(int fildes);
pid_t fork(void);
pid_t getpid(void);
pid_t getppid(void);
int setpgid(pid_t pid, pid_t pgid);
pid_t getpgid(pid_t pid);
pid_t getpgrp(void);
pid_t setsid(void);
pid_t getsid(pid_t pid);
pid_t tcgetpgrp(int fildes);
int tcsetpgrp(int fildes, pid_t pgid_id);
int execve(const char *path, char *const argv[], char *const envp[]);
int execv(const char *path, char *const argv[]);
int execvp(const char *file, char *const argv[]);
int getentropy(void *buffer, size_t length);
_Noreturn void _exit(int status);
unsigned int sleep(unsigned int seconds);

#endif /* _UNISTD_H */
