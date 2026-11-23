/**
 * @file     openssh_config.h
 * @brief    OpenSSH Portable 9.9p2 config.h for Microkernel Ring 3
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _MICROKERNEL_OPENSSH_CONFIG_H
#define _MICROKERNEL_OPENSSH_CONFIG_H

/* Package metadata. */
#define PACKAGE_NAME "OpenSSH"
#define PACKAGE_TARNAME "openssh"
#define PACKAGE_VERSION "Portable"
#define PACKAGE_STRING "OpenSSH Portable"
#define PACKAGE_BUGREPORT "openssh-unix-dev@mindrot.org"
#define PACKAGE_URL ""

/* Standard and system headers present in Microkernel sysroot. */
#define STDC_HEADERS 1
#define HAVE_DIRENT_H 1
#define HAVE_ENDIAN_H 1
#define HAVE_FCNTL_H 1
#define HAVE_FNMATCH_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_LANGINFO_H 1
#define HAVE_LIMITS_H 1
#define HAVE_LOCALE_H 1
#define HAVE_NETDB_H 1
#define HAVE_NETINET_IN_SYSTM_H 1
#define HAVE_POLL_H 1
#define HAVE_STDDEF_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_MMAN_H 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_SYS_STATVFS_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_UN_H 1
#define HAVE_TIME_H 1
#define HAVE_UNISTD_H 1
#define HAVE_WCHAR_H 1

/* LP64 x86-64 type sizes and structures. */
#define SIZEOF_SHORT_INT 2
#define SIZEOF_INT 4
#define SIZEOF_LONG_INT 8
#define SIZEOF_LONG_LONG_INT 8
#define SIZEOF_TIME_T 8

#define HAVE_CLOCK_T 1
#define HAVE_FSBLKCNT_T 1
#define HAVE_FSFILCNT_T 1
#define HAVE_IN_ADDR_T 1
#define HAVE_IN_PORT_T 1
#define HAVE_INT64_T 1
#define HAVE_INTMAX_T 1
#define HAVE_INTXX_T 1
#define HAVE_MODE_T 1
#define HAVE_NFDS_T 1
#define HAVE_PID_T 1
#define HAVE_SA_FAMILY_T 1
#define HAVE_SIG_ATOMIC_T 1
#define HAVE_SIZE_T 1
#define HAVE_SSIZE_T 1
#define HAVE_UINTMAX_T 1
#define HAVE_UINTXX_T 1
#define HAVE_UNSIGNED_LONG_LONG 1
#define HAVE_STRUCT_ADDRINFO 1
#define HAVE_STRUCT_IN6_ADDR 1
#define HAVE_STRUCT_POLLFD_FD 1
#define HAVE_STRUCT_PASSWD_PW_GECOS 1
#define HAVE_STRUCT_SOCKADDR_IN6 1
#define HAVE_STRUCT_SOCKADDR_IN6_SIN6_SCOPE_ID 1
#define HAVE_STRUCT_SOCKADDR_STORAGE 1
#define HAVE_STRUCT_STAT_ST_BLKSIZE 1
#define HAVE_STRUCT_STAT_ST_MTIM 1
#define HAVE_STRUCT_STAT_ST_MTIME 1
#define HAVE_STRUCT_TIMESPEC 1
#define HAVE_STRUCT_TIMEVAL 1
#define HAVE_SS_FAMILY_IN_SS 1
#define HAVE_CONTROL_IN_MSGHDR 1
#define HAVE_CONST_GAI_STRERROR_PROTO 1
#define HAVE_VA_COPY 1
#define HAVE___VA_COPY 1
#define HAVE___func__ 1
#define HAVE___FUNCTION__ 1
#define VARIABLE_DECLARATION_AFTER_CODE 1
#define VARIABLE_LENGTH_ARRAYS 1

/* Functions supplied by Microkernel libc. */
#define HAVE_ASPRINTF 1
#define HAVE_CALLOC 1
#define HAVE_CLOCK 1
#define HAVE_CLOCK_GETTIME 1
#define HAVE_CRYPT 1
#define HAVE_DAEMON 1
#define HAVE_DIRFD 1
#define HAVE_DLOPEN 1
#define HAVE_ENDGRENT 1
#define HAVE_FCHMOD 1
#define HAVE_FCHMODAT 1
#define HAVE_FCHOWN 1
#define HAVE_FCHOWNAT 1
#define HAVE_FNMATCH 1
#define HAVE_FREEADDRINFO 1
#define HAVE_FSTATVFS 1
#define HAVE_FUTIMES 1
#define HAVE_GAI_STRERROR 1
#define HAVE_GETADDRINFO 1
#define HAVE_GETCWD 1
#define HAVE_GETENTROPY 1
#define HAVE_GETGROUPLIST 1
#define HAVE_GETLINE 1
#define HAVE_GETNAMEINFO 1
#define HAVE_GETOPT 1
#define HAVE_GETPGID 1
#define HAVE_GETPGRP 1
#define HAVE_GETRLIMIT 1
#define HAVE_GETSID 1
#define HAVE_GETTIMEOFDAY 1
#define HAVE_INET_NTOA 1
#define HAVE_INET_NTOP 1
#define HAVE_ISBLANK 1
#define HAVE_LLABS 1
#define HAVE_LOCALTIME_R 1
#define HAVE_MBLEN 1
#define HAVE_MBTOWC 1
#define HAVE_MEMMEM 1
#define HAVE_MEMMOVE 1
#define HAVE_MKDTEMP 1
#define HAVE_MMAP 1
#define HAVE_NANOSLEEP 1
#define HAVE_NL_LANGINFO 1
#define HAVE_OPENAT 1
#define HAVE_PIPE 1
#define HAVE_POLL 1
#define HAVE_PPOLL 1
#define HAVE_PSELECT 1
#define HAVE_RAISE 1
#define HAVE_READV 1
#define HAVE_REALLOCARRAY 1
#define HAVE_REALPATH 1
#define HAVE_RECVMSG 1
#define HAVE_SENDMSG 1
#define HAVE_SETENV 1
#define HAVE_SETEGID 1
#define HAVE_SETEUID 1
#define HAVE_SETGROUPS 1
#define HAVE_SETREGID 1
#define HAVE_SETRESGID 1
#define HAVE_SETRESUID 1
#define HAVE_SETREUID 1
#define HAVE_SETRLIMIT 1
#define HAVE_SETSID 1
#define HAVE_SETVBUF 1
#define HAVE_SIGACTION 1
#define HAVE_SNPRINTF 1
#define HAVE_SOCKETPAIR 1
#define HAVE_STATVFS 1
#define HAVE_STRDUP 1
#define HAVE_STRERROR 1
#define HAVE_STRFTIME 1
#define HAVE_STRLCAT 1
#define HAVE_STRLCPY 1
#define HAVE_STRNDUP 1
#define HAVE_STRNLEN 1
#define HAVE_STRSIGNAL 1
#define HAVE_STRTOLL 1
#define HAVE_STRTOUL 1
#define HAVE_STRTOULL 1
#define HAVE_SYSCONF 1
#define HAVE_TCGETPGRP 1
#define HAVE_TCSENDBREAK 1
#define HAVE_TIME 1
#define HAVE_TRUNCATE 1
#define HAVE_UNSETENV 1
#define HAVE_UTIMENSAT 1
#define HAVE_UTIMES 1
#define HAVE_VASPRINTF 1
#define HAVE_VSNPRINTF 1
#define HAVE_WAITPID 1
#define HAVE_WCWIDTH 1

/* Declaration checks expected by openbsd-compat headers. */
#define HAVE_DECL_AI_NUMERICSERV 1
#define HAVE_DECL_BZERO 0
#define HAVE_DECL_FTRUNCATE 1
#define HAVE_DECL_GETENTROPY 1
#define HAVE_DECL_GETPEEREID 0
#define HAVE_DECL_GLOB_NOMATCH 1
#define HAVE_DECL_HOWMANY 0
#define HAVE_DECL_H_ERRNO 0
#define HAVE_DECL_MAXSYMLINKS 0
#define HAVE_DECL_MEMMEM 1
#define HAVE_DECL_NFDBITS 0
#define HAVE_DECL_OFFSETOF 1
#define HAVE_DECL_O_NONBLOCK 1
#define HAVE_DECL_READV 1
#define HAVE_DECL_SHUT_RD 1
#define HAVE_DECL_UINT32_MAX 1
#define HAVE_DECL_WRITEV 1
#define HAVE_DECL__GETLONG 0
#define HAVE_DECL__GETSHORT 0

/* Microkernel Ring 3 platform knobs (--without-openssl --without-zlib). */
#define BROKEN_GLOB 1
#define DISABLE_LASTLOG 1
#define DISABLE_LOGIN 1
#define DISABLE_SHADOW 1
#define DISABLE_UTMP 1
#define DISABLE_UTMPX 1
#define DISABLE_WTMP 1
#define DISABLE_WTMPX 1
#define GETPGRP_VOID 1
#define HAVE_DEV_PTMX 1
#define HAVE_OPENPTY 1
#define IP_TOS_IS_BROKEN 1
#define LOCKED_PASSWD_STRING "*"
#define MAIL_DIRECTORY "/tmp"
#define _PATH_MAILDIR "/tmp"
#define NO_SA_RESTART 1
#define NO_X11_UNIX_SOCKETS 1
#define SANDBOX_NULL 1
#define SPT_TYPE SPT_NONE
#define SSH_PRIVSEP_USER "sshd"
#define SUPERUSER_PATH "/boot:/bin:/tmp"
#define USER_PATH "/boot:/bin:/tmp"
#define USE_PIPES 1

/* BSD compatibility typedefs and constants for OpenSSH sources. */
#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>

typedef unsigned short u_short;
typedef unsigned long u_long;
typedef char *caddr_t;

#ifndef EPFNOSUPPORT
#define EPFNOSUPPORT EAFNOSUPPORT
#endif

#ifndef PF_UNSPEC
#define PF_UNSPEC AF_UNSPEC
#define PF_UNIX AF_UNIX
#define PF_INET AF_INET
#define PF_INET6 AF_INET6
#endif

#ifndef ITIMER_REAL
#define ITIMER_REAL 0
struct itimerval {
    struct timeval it_interval;
    struct timeval it_value;
};
int setitimer(int which, const struct itimerval *value,
              struct itimerval *ovalue);
#endif

#endif /* _MICROKERNEL_OPENSSH_CONFIG_H */
