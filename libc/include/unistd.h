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
/* need:: ICD_PROC_0009 ICD_PROC_0010 ICD_IPC_0016 */

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
#define POSIX_CLOSE_RESTART 0

/* POSIX.1-2024 sysconf() names (strict Base/CX set per BDR_0004). */
#define _SC_ARG_MAX 1
#define _SC_CHILD_MAX 2
#define _SC_CLK_TCK 3
#define _SC_NGROUPS_MAX 4
#define _SC_OPEN_MAX 5
#define _SC_STREAM_MAX 6
#define _SC_TZNAME_MAX 7
#define _SC_JOB_CONTROL 8
#define _SC_SAVED_IDS 9
#define _SC_VERSION 10
#define _SC_PAGESIZE 11
#define _SC_PAGE_SIZE _SC_PAGESIZE
#define _SC_2_VERSION 12
#define _SC_2_C_BIND 13
#define _SC_2_C_DEV 14
#define _SC_2_CHAR_TERM 15
#define _SC_2_LOCALEDEF 16
#define _SC_2_SW_DEV 17
#define _SC_2_UPE 18
#define _SC_BC_BASE_MAX 19
#define _SC_BC_DIM_MAX 20
#define _SC_BC_SCALE_MAX 21
#define _SC_BC_STRING_MAX 22
#define _SC_COLL_WEIGHTS_MAX 23
#define _SC_EXPR_NEST_MAX 24
#define _SC_LINE_MAX 25
#define _SC_RE_DUP_MAX 26
#define _SC_ASYNCHRONOUS_IO 27
#define _SC_FSYNC 28
#define _SC_MAPPED_FILES 29
#define _SC_MEMLOCK 30
#define _SC_MEMLOCK_RANGE 31
#define _SC_MEMORY_PROTECTION 32
#define _SC_MESSAGE_PASSING 33
#define _SC_PRIORITIZED_IO 34
#define _SC_PRIORITY_SCHEDULING 35
#define _SC_REALTIME_SIGNALS 36
#define _SC_SEMAPHORES 37
#define _SC_SHARED_MEMORY_OBJECTS 38
#define _SC_SYNCHRONIZED_IO 39
#define _SC_TIMERS 40
#define _SC_THREADS 41
#define _SC_THREAD_SAFE_FUNCTIONS 42
#define _SC_THREAD_DESTRUCTOR_ITERATIONS 43
#define _SC_THREAD_KEYS_MAX 44
#define _SC_THREAD_STACK_MIN 45
#define _SC_THREAD_THREADS_MAX 46
#define _SC_MONOTONIC_CLOCK 47
#define _SC_CLOCK_SELECTION 48
#define _SC_BARRIERS 49
#define _SC_READER_WRITER_LOCKS 50
#define _SC_SPIN_LOCKS 51
#define _SC_SPAWN 52
#define _SC_HOST_NAME_MAX 53
#define _SC_LOGIN_NAME_MAX 54
#define _SC_TTY_NAME_MAX 55
#define _SC_SYMLOOP_MAX 56
#define _SC_IOV_MAX 57
#define _SC_ATEXIT_MAX 58
#define _SC_GETGR_R_SIZE_MAX 59
#define _SC_GETPW_R_SIZE_MAX 60
#define _SC_V8_LP64_OFF64 61

/* POSIX.1-2024 confstr() names. */
#define _CS_PATH 1
#define _CS_POSIX_V8_LP64_OFF64_CFLAGS 2
#define _CS_POSIX_V8_LP64_OFF64_LDFLAGS 3
#define _CS_POSIX_V8_LP64_OFF64_LIBS 4
#define _CS_POSIX_V8_WIDTH_RESTRICTED_ENVS 5
#define _CS_V8_ENV 6

/* POSIX.1-2024 pathconf() / fpathconf() names. */
#define _PC_LINK_MAX 1
#define _PC_MAX_CANON 2
#define _PC_MAX_INPUT 3
#define _PC_NAME_MAX 4
#define _PC_PATH_MAX 5
#define _PC_PIPE_BUF 6
#define _PC_CHOWN_RESTRICTED 7
#define _PC_NO_TRUNC 8
#define _PC_VDISABLE 9
#define _PC_ASYNC_IO 10
#define _PC_PRIO_IO 11
#define _PC_SYNC_IO 12
#define _PC_FILESIZEBITS 13
#define _PC_SYMLINK_MAX 14
#define _PC_2_SYMLINKS 15

extern char **environ;
extern char *optarg;
extern int opterr;
extern int optind;
extern int optopt;

unsigned int alarm(unsigned int seconds);
int getopt(int argc, char *const argv[], const char *optstring);
ssize_t write(int fd, const void *buf, size_t n);
ssize_t read(int fd, void *buf, size_t n);
ssize_t pread(int fildes, void *buf, size_t nbyte, off_t offset);
ssize_t pwrite(int fildes, const void *buf, size_t nbyte, off_t offset);
off_t lseek(int fildes, off_t offset, int whence);
int ftruncate(int fildes, off_t length);
int truncate(const char *path, off_t length);
int close(int fd);
int posix_close(int fildes, int flag);
int dup(int oldfd);
int dup2(int oldfd, int newfd);
int dup3(int oldfd, int newfd, int flags);
int pipe(int fildes[2]);
int pipe2(int fildes[2], int flags);
int link(const char *path1, const char *path2);
int linkat(int fd1, const char *path1, int fd2, const char *path2, int flag);
int symlink(const char *path1, const char *path2);
int symlinkat(const char *path1, int fd, const char *path2);
ssize_t readlink(const char *restrict path, char *restrict buf, size_t bufsize);
ssize_t readlinkat(int fd, const char *restrict path, char *restrict buf,
                   size_t bufsize);
int chown(const char *path, uid_t owner, gid_t group);
int fchown(int fildes, uid_t owner, gid_t group);
int lchown(const char *path, uid_t owner, gid_t group);
int fchownat(int fd, const char *path, uid_t owner, gid_t group, int flag);
int unlink(const char *path);
int unlinkat(int fd, const char *path, int flag);
int rmdir(const char *path);
int chdir(const char *path);
int fchdir(int fildes);
char *getcwd(char *buf, size_t size);
int access(const char *path, int amode);
int faccessat(int fd, const char *path, int amode, int flag);
int isatty(int fildes);
pid_t fork(void);
pid_t getpid(void);
pid_t getppid(void);
uid_t getuid(void);
uid_t geteuid(void);
int setuid(uid_t uid);
int seteuid(uid_t uid);
gid_t getgid(void);
gid_t getegid(void);
int setgid(gid_t gid);
int setegid(gid_t gid);
int getgroups(int gidsetsize, gid_t grouplist[]);
char *getlogin(void);
int getlogin_r(char *name, size_t namesize);
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
int execl(const char *path, const char *arg0, ...);
int execle(const char *path, const char *arg0, ...);
int execlp(const char *file, const char *arg0, ...);
int fexecve(int fildes, char *const argv[], char *const envp[]);
pid_t _Fork(void);
int pause(void);
int getentropy(void *buffer, size_t length);
long sysconf(int name);
size_t confstr(int name, char *buf, size_t len);
long pathconf(const char *path, int name);
long fpathconf(int fildes, int name);
int gethostname(char *name, size_t namelen);
char *ttyname(int fildes);
int ttyname_r(int fildes, char *name, size_t namesize);
_Noreturn void _exit(int status);
unsigned int sleep(unsigned int seconds);

#endif /* _UNISTD_H */
