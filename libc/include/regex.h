/**
 * @file     regex.h
 * @brief    POSIX.1-2024 regular expression compilation and matching APIs
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _REGEX_H
#define _REGEX_H

/* need:: ICD_LIBC_0014 */

#include <stddef.h>
#include <sys/types.h>

typedef ssize_t regoff_t;

typedef struct {
    size_t re_nsub;
    void *__impl;
} regex_t;

typedef struct {
    regoff_t rm_so;
    regoff_t rm_eo;
} regmatch_t;

/* regcomp() flags. */
#define REG_EXTENDED 0x01
#define REG_ICASE 0x02
#define REG_NOSUB 0x04
#define REG_NEWLINE 0x08

/* regexec() flags. */
#define REG_NOTBOL 0x01
#define REG_NOTEOL 0x02

/* Error return codes. */
#define REG_NOMATCH 1
#define REG_BADPAT 2
#define REG_ECOLLATE 3
#define REG_ECTYPE 4
#define REG_EESCAPE 5
#define REG_ESUBREG 6
#define REG_EBRACK 7
#define REG_EPAREN 8
#define REG_EBRACE 9
#define REG_BADBR 10
#define REG_ERANGE 11
#define REG_ESPACE 12
#define REG_BADRPT 13

int regcomp(regex_t *restrict preg, const char *restrict pattern, int cflags);
int regexec(const regex_t *restrict preg, const char *restrict string,
            size_t nmatch, regmatch_t pmatch[restrict], int eflags);
size_t regerror(int errcode, const regex_t *restrict preg,
                char *restrict errbuf, size_t errbuf_size);
void regfree(regex_t *preg);

#endif /* _REGEX_H */
