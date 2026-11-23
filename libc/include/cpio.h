/**
 * @file     cpio.h
 * @brief    POSIX.1-2024 cpio archive format header constants
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CPIO_H
#define _CPIO_H

/* need:: ICD_LIBC_0014 */

#define MAGIC "070707"

/* Mode field bit definitions. */
#define C_IRUSR 0000400
#define C_IWUSR 0000200
#define C_IXUSR 0000100
#define C_IRGRP 0000040
#define C_IWGRP 0000020
#define C_IXGRP 0000010
#define C_IROTH 0000004
#define C_IWOTH 0000002
#define C_IXOTH 0000001
#define C_ISUID 0004000
#define C_ISGID 0002000
#define C_ISVTX 0001000
#define C_ISDIR 0040000
#define C_ISFIFO 0010000
#define C_ISREG 0100000
#define C_ISBLK 0060000
#define C_ISCHR 0020000
#define C_ISCTG 0110000
#define C_ISLNK 0120000
#define C_ISSOCK 0140000

#endif /* _CPIO_H */
