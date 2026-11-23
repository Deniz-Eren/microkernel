/**
 * @file     time_cal.c
 * @brief    POSIX.1-2024 calendar time, mktime, asctime, ctime, and strftime
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* need:: COMP_LIBC_0009 */

static const char *const k_wday_abbr[7] = {"Sun", "Mon", "Tue", "Wed",
                                           "Thu", "Fri", "Sat"};
static const char *const k_wday_full[7] = {
    "Sunday",   "Monday", "Tuesday",  "Wednesday",
    "Thursday", "Friday", "Saturday",
};
static const char *const k_mon_abbr[12] = {"Jan", "Feb", "Mar", "Apr",
                                           "May", "Jun", "Jul", "Aug",
                                           "Sep", "Oct", "Nov", "Dec"};
static const char *const k_mon_full[12] = {
    "January", "February", "March",     "April",   "May",      "June",
    "July",    "August",   "September", "October", "November", "December",
};
static const int k_mdays_cum[12] = {0,   31,  59,  90,  120, 151,
                                    181, 212, 243, 273, 304, 334};

static int is_leap_year(int64_t y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static int64_t floor_div(int64_t a, int64_t b) {
    int64_t q = a / b;
    int64_t r = a % b;
    if (r < 0) {
        q--;
    }
    return q;
}

static int64_t days_from_civil(int64_t y, int64_t m, int64_t d) {
    y -= (m <= 2) ? 1 : 0;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    uint64_t yoe = (uint64_t)(y - era * 400);
    uint64_t mp = (uint64_t)(m + (m > 2 ? -3 : 9));
    uint64_t doy = (153u * mp + 2u) / 5u + (uint64_t)(d - 1);
    uint64_t doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

static int secs_to_tm(int64_t sec, long gmtoff, const char *zone,
                      struct tm *out) {
    int64_t days = floor_div(sec, 86400);
    int64_t rem = sec - days * 86400;

    int64_t z = days + 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    uint64_t doe = (uint64_t)(z - era * 146097);
    uint64_t yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
    int64_t y = (int64_t)yoe + era * 400;
    uint64_t doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
    uint64_t mp = (5u * doy + 2u) / 153u;
    uint64_t d = doy - (153u * mp + 2u) / 5u + 1u;
    uint64_t m = mp + (mp < 10u ? 3u : (uint64_t)-9);
    y += (m <= 2u) ? 1 : 0;

    int64_t tm_year = y - 1900;
    if (tm_year < INT_MIN || tm_year > INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }

    int wday = (int)((days + 4) % 7);
    if (wday < 0) {
        wday += 7;
    }
    int yday = k_mdays_cum[m - 1u] + (int)(d - 1u) +
               ((m > 2u && is_leap_year(y)) ? 1 : 0);

    out->tm_sec = (int)(rem % 60);
    out->tm_min = (int)((rem / 60) % 60);
    out->tm_hour = (int)(rem / 3600);
    out->tm_mday = (int)d;
    out->tm_mon = (int)(m - 1u);
    out->tm_year = (int)tm_year;
    out->tm_wday = wday;
    out->tm_yday = yday;
    out->tm_isdst = 0;
    out->tm_gmtoff = gmtoff;
    out->tm_zone = zone;
    return 0;
}

static char g_tz_name[16] = "UTC";

static void parse_tz(long *gmtoff_out, const char **zone_out) {
    const char *tz = getenv("TZ");
    if (!tz || !*tz) {
        *gmtoff_out = 0;
        *zone_out = "UTC";
        return;
    }
    if (*tz == ':') {
        tz++;
    }
    size_t nlen = 0;
    while (tz[nlen] && isalpha((unsigned char)tz[nlen]) &&
           nlen < sizeof(g_tz_name) - 1u) {
        g_tz_name[nlen] = tz[nlen];
        nlen++;
    }
    if (nlen == 0u) {
        *gmtoff_out = 0;
        *zone_out = "UTC";
        return;
    }
    g_tz_name[nlen] = '\0';
    const char *p = tz + nlen;
    if (!*p) {
        *gmtoff_out = 0;
        *zone_out = g_tz_name;
        return;
    }

    /* POSIX TZ sign: positive offset is WEST of UTC (negative tm_gmtoff). */
    int west = 1;
    if (*p == '+') {
        p++;
    } else if (*p == '-') {
        west = 0;
        p++;
    }
    long hh = 0;
    long mm = 0;
    long ss = 0;
    if (!isdigit((unsigned char)*p)) {
        *gmtoff_out = 0;
        *zone_out = g_tz_name;
        return;
    }
    while (isdigit((unsigned char)*p)) {
        hh = hh * 10 + (*p++ - '0');
    }
    if (*p == ':') {
        p++;
        while (isdigit((unsigned char)*p)) {
            mm = mm * 10 + (*p++ - '0');
        }
        if (*p == ':') {
            p++;
            while (isdigit((unsigned char)*p)) {
                ss = ss * 10 + (*p++ - '0');
            }
        }
    }
    long total = hh * 3600L + mm * 60L + ss;
    *gmtoff_out = west ? -total : total;
    *zone_out = g_tz_name;
}

struct tm *gmtime_r(const time_t *restrict timer, struct tm *restrict result) {
    if (!timer || !result) {
        errno = EINVAL;
        return 0;
    }
    if (secs_to_tm((int64_t)*timer, 0L, "UTC", result) != 0) {
        return 0;
    }
    return result;
}

struct tm *gmtime(const time_t *timer) {
    static struct tm g_gmtime_buf;
    return gmtime_r(timer, &g_gmtime_buf);
}

struct tm *localtime_r(const time_t *restrict timer,
                       struct tm *restrict result) {
    if (!timer || !result) {
        errno = EINVAL;
        return 0;
    }
    long gmtoff = 0L;
    const char *zone = "UTC";
    parse_tz(&gmtoff, &zone);
    if (secs_to_tm((int64_t)*timer + (int64_t)gmtoff, gmtoff, zone, result) !=
        0) {
        return 0;
    }
    return result;
}

struct tm *localtime(const time_t *timer) {
    static struct tm g_localtime_buf;
    return localtime_r(timer, &g_localtime_buf);
}

time_t mktime(struct tm *timeptr) {
    if (!timeptr) {
        errno = EINVAL;
        return (time_t)-1;
    }
    int64_t sec = timeptr->tm_sec;
    int64_t min = timeptr->tm_min;
    int64_t hour = timeptr->tm_hour;
    int64_t mday = timeptr->tm_mday;
    int64_t mon = timeptr->tm_mon;
    int64_t year = (int64_t)timeptr->tm_year + 1900;

    int64_t c_min = floor_div(sec, 60);
    sec -= c_min * 60;
    min += c_min;

    int64_t c_hour = floor_div(min, 60);
    min -= c_hour * 60;
    hour += c_hour;

    int64_t c_day = floor_div(hour, 24);
    hour -= c_day * 24;
    mday += c_day;

    int64_t c_year = floor_div(mon, 12);
    mon -= c_year * 12;
    year += c_year;

    int64_t days = days_from_civil(year, mon + 1, 1) + (mday - 1);
    int64_t local_sec = days * 86400 + hour * 3600 + min * 60 + sec;

    long gmtoff = 0L;
    const char *zone = "UTC";
    parse_tz(&gmtoff, &zone);
    int64_t utc_sec = local_sec - (int64_t)gmtoff;

    time_t res = (time_t)utc_sec;
    if (secs_to_tm(local_sec, gmtoff, zone, timeptr) != 0) {
        return (time_t)-1;
    }
    return res;
}

char *asctime_r(const struct tm *restrict tm, char *restrict buf) {
    if (!tm || !buf) {
        errno = EINVAL;
        return 0;
    }
    const char *wday = (tm->tm_wday >= 0 && tm->tm_wday < 7)
                           ? k_wday_abbr[tm->tm_wday]
                           : "???";
    const char *mon =
        (tm->tm_mon >= 0 && tm->tm_mon < 12) ? k_mon_abbr[tm->tm_mon] : "???";
    (void)snprintf(buf, 26u, "%.3s %.3s%3d %02d:%02d:%02d %d\n", wday, mon,
                   tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec,
                   tm->tm_year + 1900);
    return buf;
}

char *asctime(const struct tm *timeptr) {
    static char g_asctime_buf[32];
    return asctime_r(timeptr, g_asctime_buf);
}

char *ctime_r(const time_t *clock_val, char *buf) {
    struct tm tmp;
    if (!localtime_r(clock_val, &tmp)) {
        return 0;
    }
    return asctime_r(&tmp, buf);
}

char *ctime(const time_t *clock_val) {
    struct tm *tp = localtime(clock_val);
    if (!tp) {
        return 0;
    }
    return asctime(tp);
}

struct sf_out {
    char *buf;
    size_t cap;
    size_t pos;
    int overflow;
};

static void sf_putc(struct sf_out *o, char c) {
    if (o->pos + 1u < o->cap) {
        o->buf[o->pos++] = c;
    } else {
        o->overflow = 1;
    }
}

static void sf_puts(struct sf_out *o, const char *s) {
    while (*s) {
        sf_putc(o, *s++);
    }
}

static void sf_num(struct sf_out *o, long long v, int width, char pad) {
    char tmp[32];
    int neg = 0;
    unsigned long long mag = 0ULL;
    if (v < 0) {
        neg = 1;
        mag = (unsigned long long)(-(v + 1)) + 1ULL;
    } else {
        mag = (unsigned long long)v;
    }
    int n = 0;
    do {
        tmp[n++] = (char)('0' + (mag % 10ULL));
        mag /= 10ULL;
    } while (mag != 0ULL);

    int total = n + (neg ? 1 : 0);
    if (neg && pad == '0') {
        sf_putc(o, '-');
    }
    for (int i = total; i < width; i++) {
        sf_putc(o, pad);
    }
    if (neg && pad != '0') {
        sf_putc(o, '-');
    }
    while (n > 0) {
        sf_putc(o, tmp[--n]);
    }
}

static int iso_week_info(const struct tm *tm, int *iso_year_out) {
    int y = tm->tm_year + 1900;
    int yday = tm->tm_yday;
    int wday = tm->tm_wday == 0 ? 7 : tm->tm_wday;
    int week = (yday - wday + 10) / 7;
    if (week < 1) {
        int prev_y = y - 1;
        int prev_days = is_leap_year(prev_y) ? 366 : 365;
        int prev_yday = yday + prev_days;
        week = (prev_yday - wday + 10) / 7;
        *iso_year_out = prev_y;
        return week;
    }
    if (week == 53) {
        int days_in_y = is_leap_year(y) ? 366 : 365;
        if (days_in_y - yday < 4 - wday) {
            *iso_year_out = y + 1;
            return 1;
        }
    }
    *iso_year_out = y;
    return week;
}

static void sf_format(struct sf_out *o, const char *fmt, const struct tm *tm) {
    for (const char *f = fmt; *f && !o->overflow; f++) {
        if (*f != '%') {
            sf_putc(o, *f);
            continue;
        }
        f++;
        if (*f == 'E' || *f == 'O') {
            f++;
        }
        if (!*f) {
            break;
        }
        int year = tm->tm_year + 1900;
        switch (*f) {
        case '%':
            sf_putc(o, '%');
            break;
        case 'a':
            sf_puts(o, (tm->tm_wday >= 0 && tm->tm_wday < 7)
                           ? k_wday_abbr[tm->tm_wday]
                           : "?");
            break;
        case 'A':
            sf_puts(o, (tm->tm_wday >= 0 && tm->tm_wday < 7)
                           ? k_wday_full[tm->tm_wday]
                           : "?");
            break;
        case 'b':
        case 'h':
            sf_puts(o, (tm->tm_mon >= 0 && tm->tm_mon < 12)
                           ? k_mon_abbr[tm->tm_mon]
                           : "?");
            break;
        case 'B':
            sf_puts(o, (tm->tm_mon >= 0 && tm->tm_mon < 12)
                           ? k_mon_full[tm->tm_mon]
                           : "?");
            break;
        case 'c':
            sf_format(o, "%a %b %e %H:%M:%S %Y", tm);
            break;
        case 'C':
            sf_num(o, floor_div(year, 100), 2, '0');
            break;
        case 'd':
            sf_num(o, tm->tm_mday, 2, '0');
            break;
        case 'D':
            sf_format(o, "%m/%d/%y", tm);
            break;
        case 'e':
            sf_num(o, tm->tm_mday, 2, ' ');
            break;
        case 'F':
            sf_format(o, "%Y-%m-%d", tm);
            break;
        case 'g':
        case 'G':
        case 'V': {
            int iy = 0;
            int iw = iso_week_info(tm, &iy);
            if (*f == 'V') {
                sf_num(o, iw, 2, '0');
            } else if (*f == 'G') {
                sf_num(o, iy, 4, '0');
            } else {
                int y2 = iy % 100;
                if (y2 < 0) {
                    y2 += 100;
                }
                sf_num(o, y2, 2, '0');
            }
            break;
        }
        case 'H':
            sf_num(o, tm->tm_hour, 2, '0');
            break;
        case 'I': {
            int h12 = tm->tm_hour % 12;
            sf_num(o, h12 == 0 ? 12 : h12, 2, '0');
            break;
        }
        case 'j':
            sf_num(o, tm->tm_yday + 1, 3, '0');
            break;
        case 'm':
            sf_num(o, tm->tm_mon + 1, 2, '0');
            break;
        case 'M':
            sf_num(o, tm->tm_min, 2, '0');
            break;
        case 'n':
            sf_putc(o, '\n');
            break;
        case 'p':
            sf_puts(o, tm->tm_hour < 12 ? "AM" : "PM");
            break;
        case 'r':
            sf_format(o, "%I:%M:%S %p", tm);
            break;
        case 'R':
            sf_format(o, "%H:%M", tm);
            break;
        case 's': {
            struct tm copy = *tm;
            time_t epoch = mktime(&copy);
            sf_num(o, (long long)epoch, 1, '0');
            break;
        }
        case 'S':
            sf_num(o, tm->tm_sec, 2, '0');
            break;
        case 't':
            sf_putc(o, '\t');
            break;
        case 'T':
            sf_format(o, "%H:%M:%S", tm);
            break;
        case 'u':
            sf_num(o, tm->tm_wday == 0 ? 7 : tm->tm_wday, 1, '0');
            break;
        case 'U':
            sf_num(o, (tm->tm_yday + 7 - tm->tm_wday) / 7, 2, '0');
            break;
        case 'w':
            sf_num(o, tm->tm_wday, 1, '0');
            break;
        case 'W': {
            int mon_wday = (tm->tm_wday + 6) % 7;
            sf_num(o, (tm->tm_yday + 7 - mon_wday) / 7, 2, '0');
            break;
        }
        case 'x':
            sf_format(o, "%m/%d/%y", tm);
            break;
        case 'X':
            sf_format(o, "%H:%M:%S", tm);
            break;
        case 'y': {
            int y2 = year % 100;
            if (y2 < 0) {
                y2 += 100;
            }
            sf_num(o, y2, 2, '0');
            break;
        }
        case 'Y':
            sf_num(o, year, 4, '0');
            break;
        case 'z': {
            long off = tm->tm_gmtoff;
            sf_putc(o, off < 0 ? '-' : '+');
            if (off < 0) {
                off = -off;
            }
            sf_num(o, off / 3600L, 2, '0');
            sf_num(o, (off % 3600L) / 60L, 2, '0');
            break;
        }
        case 'Z':
            sf_puts(o, tm->tm_zone ? tm->tm_zone : "UTC");
            break;
        default:
            sf_putc(o, '%');
            sf_putc(o, *f);
            break;
        }
    }
}

size_t strftime(char *restrict s, size_t maxsize, const char *restrict format,
                const struct tm *restrict timeptr) {
    if (!s || maxsize == 0u || !format || !timeptr) {
        return 0u;
    }
    struct sf_out out = {.buf = s, .cap = maxsize, .pos = 0u, .overflow = 0};
    sf_format(&out, format, timeptr);
    if (out.overflow) {
        return 0u;
    }
    out.buf[out.pos] = '\0';
    return out.pos;
}
