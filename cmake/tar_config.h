/**
 * @file     tar_config.h
 * @brief    libarchive 3.8.2 config.h for Microkernel Ring 3
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _MICROKERNEL_TAR_CONFIG_H
#define _MICROKERNEL_TAR_CONFIG_H

/* Marker that config.h was included first (archive_digest_private.h). */
#define __LIBARCHIVE_CONFIG_H_INCLUDED 1

/* Package metadata. */
#define PACKAGE_NAME "libarchive"
#define PACKAGE_TARNAME "libarchive"
#define PACKAGE_VERSION "3.8.2"
#define PACKAGE_STRING "libarchive 3.8.2"
#define PACKAGE_BUGREPORT "libarchive-discuss@googlegroups.com"
#define PACKAGE_URL ""
#define VERSION "3.8.2"
#define BSDTAR_VERSION_STRING "3.8.2"
#define LIBARCHIVE_VERSION_NUMBER "3008002"
#define LIBARCHIVE_VERSION_STRING "3.8.2"

/* Only zlib is available in the Ring 3 sysroot. All other optional
 * compression, digest, ACL, and xattr backends stay disabled. */
#define HAVE_ZLIB_H 1
#define HAVE_LIBZ 1

/* Headers present in the Microkernel sysroot. */
#define HAVE_CTYPE_H 1
#define HAVE_DIRENT_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_FNMATCH_H 1
#define HAVE_GRP_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_LANGINFO_H 1
#define HAVE_LIMITS_H 1
#define HAVE_LOCALE_H 1
#define HAVE_MEMORY_H 1
#define HAVE_POLL_H 1
#define HAVE_PWD_H 1
#define HAVE_REGEX_H 1
#define HAVE_SIGNAL_H 1
#define HAVE_STDARG_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_SYS_IOCTL_H 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_STATVFS_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_UTSNAME_H 1
#define HAVE_SYS_WAIT_H 1
#define HAVE_TIME_H 1
#define HAVE_UNISTD_H 1
#define HAVE_WCHAR_H 1
#define HAVE_WCHAR_T 1
#define HAVE_WCTYPE_H 1
#define TIME_WITH_SYS_TIME 1

/* C types. */
#define HAVE_INT16_T 1
#define HAVE_INT32_T 1
#define HAVE_INT64_T 1
#define HAVE_INTMAX_T 1
#define HAVE_LONG_LONG_INT 1
#define HAVE_UINT8_T 1
#define HAVE_UINT16_T 1
#define HAVE_UINT32_T 1
#define HAVE_UINT64_T 1
#define HAVE_UINTMAX_T 1
#define HAVE_UNSIGNED_LONG_LONG 1
#define HAVE_UNSIGNED_LONG_LONG_INT 1
#define SIZEOF_WCHAR_T 4

/* Macros declared by the sysroot headers (no fallback redefinitions). */
#define HAVE_DECL_INT32_MAX 1
#define HAVE_DECL_INT32_MIN 1
#define HAVE_DECL_INT64_MAX 1
#define HAVE_DECL_INT64_MIN 1
#define HAVE_DECL_INTMAX_MAX 1
#define HAVE_DECL_INTMAX_MIN 1
#define HAVE_DECL_SIZE_MAX 1
#define HAVE_DECL_SSIZE_MAX 1
#define HAVE_DECL_STRERROR_R 1
#define HAVE_DECL_UINT32_MAX 1
#define HAVE_DECL_UINT64_MAX 1
#define HAVE_DECL_UINTMAX_MAX 1

/* POSIX functions provided by the Microkernel libc. */
#define HAVE_CTIME_R 1
#define HAVE_DIRFD 1
#define HAVE_FDOPENDIR 1
#define HAVE_FCHDIR 1
#define HAVE_FCHMOD 1
#define HAVE_FCHOWN 1
#define HAVE_CHOWN 1
#define HAVE_FCNTL 1
#define HAVE_FNMATCH 1
#define HAVE_FORK 1
#define HAVE_FSTAT 1
#define HAVE_FSTATAT 1
#define HAVE_FSTATVFS 1
#define HAVE_FTRUNCATE 1
#define HAVE_FUTIMENS 1
#define HAVE_FUTIMES 1
#define HAVE_GETEUID 1
#define HAVE_GETGRGID_R 1
#define HAVE_GETGRNAM_R 1
#define HAVE_GETLINE 1
#define HAVE_GETPID 1
#define HAVE_GETPWNAM_R 1
#define HAVE_GETPWUID_R 1
#define HAVE_GMTIME_R 1
#define HAVE_LCHOWN 1
#define HAVE_LINK 1
#define HAVE_LINKAT 1
#define HAVE_LOCALTIME_R 1
#define HAVE_LSTAT 1
#define HAVE_MBRTOWC 1
#define HAVE_MEMMOVE 1
#define HAVE_MKDIR 1
#define HAVE_MKFIFO 1
#define HAVE_MKSTEMP 1
#define HAVE_NL_LANGINFO 1
#define HAVE_OPENAT 1
#define HAVE_PIPE 1
#define HAVE_POLL 1
#define HAVE_READLINK 1
#define HAVE_READLINKAT 1
#define HAVE_SELECT 1
#define HAVE_SETENV 1
#define HAVE_SETLOCALE 1
#define HAVE_SIGACTION 1
#define HAVE_STATVFS 1
#define HAVE_STRCHR 1
#define HAVE_STRDUP 1
#define HAVE_STRERROR 1
#define HAVE_STRERROR_R 1
#define HAVE_STRFTIME 1
#define HAVE_STRNLEN 1
#define HAVE_STRRCHR 1
#define HAVE_SYMLINK 1
#define HAVE_SYSCONF 1
#define HAVE_TCGETATTR 1
#define HAVE_TCSETATTR 1
#define HAVE_TZSET 1
#define HAVE_UNLINKAT 1
#define HAVE_UNSETENV 1
#define HAVE_UTIMENSAT 1
#define HAVE_UTIMES 1
#define HAVE_VPRINTF 1
#define HAVE_WCRTOMB 1
#define HAVE_WCSCMP 1
#define HAVE_WCSCPY 1
#define HAVE_WCSLEN 1
#define HAVE_WCTOMB 1
#define HAVE_WMEMCMP 1
#define HAVE_WMEMCPY 1
#define HAVE_WMEMMOVE 1

/* struct fields present in the Microkernel sysroot headers. */
#define HAVE_STRUCT_STAT_ST_BLKSIZE 1
#define HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC 1
#define HAVE_STRUCT_TM_TM_GMTOFF 1

/* strerror_r() follows the POSIX/XSI int-returning form. */
/* STRERROR_R_CHAR_P stays undefined. */

/* Not provided by the Microkernel libc; libarchive falls back internally
 * (device nodes, timegm, and xattr or ACL paths error at runtime only). */
/* HAVE_MKNOD, HAVE_TIMEGM, HAVE_FUTIMESAT, HAVE_LUTIMES, HAVE_LCHMOD,
 * HAVE_CHFLAGS, HAVE_FCHFLAGS, HAVE_LCHFLAGS, HAVE_ARC4RANDOM_BUF,
 * HAVE_POSIX_SPAWNP, HAVE_STATFS, HAVE_CLOSEFROM, and HAVE_CLOSE_RANGE
 * stay undefined. */

/* No optional libraries: bzip2, lzma/xz, zstd, lz4, lzo, expat, libxml2,
 * nettle, mbedTLS, OpenSSL digests, iconv, ACL, and xattr stay disabled. */

#endif /* _MICROKERNEL_TAR_CONFIG_H */
