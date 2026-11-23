/**
 * @file     libintl.h
 * @brief    POSIX.1-2024 message catalog translation (gettext/ngettext) APIs
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _LIBINTL_H
#define _LIBINTL_H

/* need:: ICD_LIBC_0015 */

#include <sys/_locale_t.h>

char *gettext(const char *msgid);
char *gettext_l(const char *msgid, locale_t locale);
char *dgettext(const char *domainname, const char *msgid);
char *dgettext_l(const char *domainname, const char *msgid, locale_t locale);
char *dcgettext(const char *domainname, const char *msgid, int category);
char *dcgettext_l(const char *domainname, const char *msgid, int category,
                  locale_t locale);

char *ngettext(const char *msgid1, const char *msgid2, unsigned long int n);
char *ngettext_l(const char *msgid1, const char *msgid2, unsigned long int n,
                 locale_t locale);
char *dngettext(const char *domainname, const char *msgid1, const char *msgid2,
                unsigned long int n);
char *dngettext_l(const char *domainname, const char *msgid1,
                  const char *msgid2, unsigned long int n, locale_t locale);
char *dcngettext(const char *domainname, const char *msgid1, const char *msgid2,
                 unsigned long int n, int category);
char *dcngettext_l(const char *domainname, const char *msgid1,
                   const char *msgid2, unsigned long int n, int category,
                   locale_t locale);

char *textdomain(const char *domainname);
char *bindtextdomain(const char *domainname, const char *dirname);
char *bind_textdomain_codeset(const char *domainname, const char *codeset);

#endif /* _LIBINTL_H */
