/**
 * @file     langinfo.c
 * @brief    POSIX.1-2024 nl_langinfo and nl_langinfo_l implementation
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <langinfo.h>
#include <locale.h>
#include "locale_internal.h"

/* need:: COMP_LIBC_0015 */

static const char *const k_day[7] = {
    "Sunday",   "Monday", "Tuesday",  "Wednesday",
    "Thursday", "Friday", "Saturday",
};

static const char *const k_abday[7] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
};

static const char *const k_mon[12] = {
    "January", "February", "March",     "April",   "May",      "June",
    "July",    "August",   "September", "October", "November", "December",
};

static const char *const k_abmon[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

char *nl_langinfo_l(nl_item item, locale_t locale) {
    if (item == CODESET) {
        return (char *)(__libc_locale_is_utf8(locale) ? "UTF-8" : "US-ASCII");
    }
    if (item >= DAY_1 && item <= DAY_7) {
        return (char *)k_day[item - DAY_1];
    }
    if (item >= ABDAY_1 && item <= ABDAY_7) {
        return (char *)k_abday[item - ABDAY_1];
    }
    if (item >= MON_1 && item <= MON_12) {
        return (char *)k_mon[item - MON_1];
    }
    if (item >= ABMON_1 && item <= ABMON_12) {
        return (char *)k_abmon[item - ABMON_1];
    }
    if (item >= ALTMON_1 && item <= ALTMON_12) {
        return (char *)k_mon[item - ALTMON_1];
    }
    if (item >= ABALTMON_1 && item <= ABALTMON_12) {
        return (char *)k_abmon[item - ABALTMON_1];
    }
    switch (item) {
    case D_T_FMT:
        return (char *)"%a %b %e %H:%M:%S %Y";
    case D_FMT:
        return (char *)"%m/%d/%y";
    case T_FMT:
        return (char *)"%H:%M:%S";
    case T_FMT_AMPM:
        return (char *)"%I:%M:%S %p";
    case AM_STR:
        return (char *)"AM";
    case PM_STR:
        return (char *)"PM";
    case RADIXCHAR:
        return (char *)".";
    case THOUSEP:
        return (char *)"";
    case YESEXPR:
        return (char *)"^[yY]";
    case NOEXPR:
        return (char *)"^[nN]";
    case CRNCYSTR:
    case ERA:
    case ERA_D_FMT:
    case ERA_D_T_FMT:
    case ERA_T_FMT:
    case ALT_DIGITS:
    default:
        return (char *)"";
    }
}

char *nl_langinfo(nl_item item) {
    return nl_langinfo_l(item, uselocale((locale_t)0));
}
