/**
 * @file     at.c
 * @brief    Ring 3 POSIX.1-2024 XCU at deferred-job scheduling utility
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* need:: COMP_LIBC_0027 */

#define AT_SPOOL_DIR "/tmp/spool/at"
#define AT_SEQ_FILE "/tmp/spool/at/.seq"

static int ensure_spool(void) {
    if (mkdir("/tmp/spool", 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    if (mkdir(AT_SPOOL_DIR, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

static unsigned long next_job_id(void) {
    unsigned long id = 0;
    FILE *fp = fopen(AT_SEQ_FILE, "rb");
    if (fp) {
        (void)fscanf(fp, "%lu", &id);
        (void)fclose(fp);
    }
    id++;
    fp = fopen(AT_SEQ_FILE, "wb");
    if (fp) {
        (void)fprintf(fp, "%lu\n", id);
        (void)fclose(fp);
    }
    return id;
}

static int parse_touch_time(const char *s, time_t *out) {
    /* [[CC]YY]MMDDhhmm[.SS] */
    const char *dot = strchr(s, '.');
    size_t dlen = dot ? (size_t)(dot - s) : strlen(s);
    if (dlen != 8 && dlen != 10 && dlen != 12) {
        return -1;
    }
    for (size_t i = 0; i < dlen; i++) {
        if (!isdigit((unsigned char)s[i])) {
            return -1;
        }
    }
    int sec = 0;
    if (dot) {
        if (strlen(dot + 1) != 2 || !isdigit((unsigned char)dot[1]) ||
            !isdigit((unsigned char)dot[2])) {
            return -1;
        }
        sec = (dot[1] - '0') * 10 + (dot[2] - '0');
    }

    time_t now = time(NULL);
    struct tm tm_val;
    if (!gmtime_r(&now, &tm_val)) {
        (void)memset(&tm_val, 0, sizeof(tm_val));
        tm_val.tm_year = 126;
    }
    const char *p = s;
    if (dlen == 12) {
        int yr = (p[0] - '0') * 1000 + (p[1] - '0') * 100 + (p[2] - '0') * 10 +
                 (p[3] - '0');
        tm_val.tm_year = yr - 1900;
        p += 4;
    } else if (dlen == 10) {
        int yy = (p[0] - '0') * 10 + (p[1] - '0');
        tm_val.tm_year = (yy >= 69) ? yy : (100 + yy);
        p += 2;
    }
    tm_val.tm_mon = ((p[0] - '0') * 10 + (p[1] - '0')) - 1;
    tm_val.tm_mday = (p[2] - '0') * 10 + (p[3] - '0');
    tm_val.tm_hour = (p[4] - '0') * 10 + (p[5] - '0');
    tm_val.tm_min = (p[6] - '0') * 10 + (p[7] - '0');
    tm_val.tm_sec = sec;
    tm_val.tm_isdst = 0;
    *out = mktime(&tm_val);
    return (*out == (time_t)-1) ? -1 : 0;
}

static int parse_timespec(int argc, char **argv, int start, time_t *out) {
    if (start >= argc) {
        return -1;
    }
    time_t now = time(NULL);
    struct tm tm_val;
    if (!gmtime_r(&now, &tm_val)) {
        (void)memset(&tm_val, 0, sizeof(tm_val));
        tm_val.tm_year = 126;
    }

    const char *w0 = argv[start];
    if (strcmp(w0, "now") == 0) {
        time_t t = now;
        if (start + 3 < argc && strcmp(argv[start + 1], "+") == 0) {
            long n = strtol(argv[start + 2], NULL, 10);
            const char *u = argv[start + 3];
            if (strncmp(u, "minute", 6) == 0) {
                t += n * 60L;
            } else if (strncmp(u, "hour", 4) == 0) {
                t += n * 3600L;
            } else if (strncmp(u, "day", 3) == 0) {
                t += n * 86400L;
            } else {
                return -1;
            }
        }
        *out = t;
        return 0;
    }
    if (strcmp(w0, "noon") == 0) {
        tm_val.tm_hour = 12;
        tm_val.tm_min = 0;
        tm_val.tm_sec = 0;
        *out = mktime(&tm_val);
        return 0;
    }
    if (strcmp(w0, "midnight") == 0) {
        tm_val.tm_hour = 0;
        tm_val.tm_min = 0;
        tm_val.tm_sec = 0;
        *out = mktime(&tm_val);
        return 0;
    }
    const char *colon = strchr(w0, ':');
    if (colon) {
        int hh = atoi(w0);
        int mm = atoi(colon + 1);
        if (hh < 0 || hh > 23 || mm < 0 || mm > 59) {
            return -1;
        }
        tm_val.tm_hour = hh;
        tm_val.tm_min = mm;
        tm_val.tm_sec = 0;
        *out = mktime(&tm_val);
        return 0;
    }
    if (strlen(w0) == 4 && isdigit((unsigned char)w0[0]) &&
        isdigit((unsigned char)w0[1]) && isdigit((unsigned char)w0[2]) &&
        isdigit((unsigned char)w0[3])) {
        int hh = (w0[0] - '0') * 10 + (w0[1] - '0');
        int mm = (w0[2] - '0') * 10 + (w0[3] - '0');
        if (hh > 23 || mm > 59) {
            return -1;
        }
        tm_val.tm_hour = hh;
        tm_val.tm_min = mm;
        tm_val.tm_sec = 0;
        *out = mktime(&tm_val);
        return 0;
    }
    return -1;
}

static int list_jobs(char qfilter, int argc, char **argv, int start) {
    DIR *d = opendir(AT_SPOOL_DIR);
    if (!d) {
        return 0;
    }
    struct dirent *de = NULL;
    while ((de = readdir(d)) != NULL) {
        if (strncmp(de->d_name, "job.", 4) != 0) {
            continue;
        }
        const char *ext = strstr(de->d_name, ".meta");
        if (!ext || ext[5] != '\0') {
            continue;
        }
        unsigned long id = strtoul(de->d_name + 4, NULL, 10);
        if (start < argc) {
            int want = 0;
            for (int i = start; i < argc; i++) {
                if (strtoul(argv[i], NULL, 10) == id) {
                    want = 1;
                    break;
                }
            }
            if (!want) {
                continue;
            }
        }
        char path[256];
        (void)snprintf(path, sizeof(path), "%s/%s", AT_SPOOL_DIR, de->d_name);
        FILE *fp = fopen(path, "rb");
        if (!fp) {
            continue;
        }
        char q = 'a';
        char tstr[64];
        tstr[0] = '\0';
        char line[128];
        while (fgets(line, sizeof(line), fp) != NULL) {
            size_t l = strlen(line);
            while (l > 0 && (line[l - 1] == '\n' || line[l - 1] == '\r')) {
                line[--l] = '\0';
            }
            if (strncmp(line, "queue=", 6) == 0 && line[6] != '\0') {
                q = line[6];
            } else if (strncmp(line, "time=", 5) == 0) {
                (void)snprintf(tstr, sizeof(tstr), "%s", line + 5);
            }
        }
        (void)fclose(fp);
        if (qfilter != '\0' && q != qfilter) {
            continue;
        }
        (void)printf("%lu\t%s %c\n", id, tstr, q);
    }
    (void)closedir(d);
    return 0;
}

static int remove_jobs(int argc, char **argv, int start) {
    int rc = 0;
    for (int i = start; i < argc; i++) {
        unsigned long id = strtoul(argv[i], NULL, 10);
        char mp[256];
        char sp[256];
        (void)snprintf(mp, sizeof(mp), "%s/job.%lu.meta", AT_SPOOL_DIR, id);
        (void)snprintf(sp, sizeof(sp), "%s/job.%lu.sh", AT_SPOOL_DIR, id);
        if (unlink(mp) != 0) {
            (void)fprintf(stderr, "at: cannot remove job %lu\n", id);
            rc = 1;
        }
        (void)unlink(sp);
    }
    return rc;
}

int main(int argc, char **argv) {
    int mode_list = 0;
    int mode_rem = 0;
    int send_mail = 0;
    char queue = 'a';
    const char *file_arg = NULL;
    const char *time_arg = NULL;
    int opt = 0;

    while ((opt = getopt(argc, argv, "f:lmq:rt:")) != -1) {
        switch (opt) {
        case 'f':
            file_arg = optarg;
            break;
        case 'l':
            mode_list = 1;
            break;
        case 'm':
            send_mail = 1;
            break;
        case 'q':
            if (optarg[0] < 'a' || optarg[0] > 'z' || optarg[1] != '\0') {
                (void)fprintf(stderr, "at: invalid queue: %s\n", optarg);
                return 1;
            }
            queue = optarg[0];
            break;
        case 'r':
            mode_rem = 1;
            break;
        case 't':
            time_arg = optarg;
            break;
        default:
            (void)fprintf(stderr, "usage: at [-m] [-f file] [-q q] [-t time | "
                                  "timespec] | at -l | at -r id...\n");
            return 1;
        }
    }

    if (ensure_spool() != 0) {
        perror("at: spool");
        return 1;
    }
    if (mode_list) {
        char qf = (queue != 'a') ? queue : '\0';
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-q") == 0 || strncmp(argv[i], "-q", 2) == 0) {
                qf = queue;
                break;
            }
        }
        return list_jobs(qf, argc, argv, optind);
    }
    if (mode_rem) {
        if (optind >= argc) {
            (void)fprintf(stderr, "at: -r requires job id\n");
            return 1;
        }
        return remove_jobs(argc, argv, optind);
    }

    time_t when = 0;
    int trc = time_arg ? parse_touch_time(time_arg, &when)
                       : parse_timespec(argc, argv, optind, &when);
    if (trc != 0) {
        (void)fprintf(stderr, "at: invalid time specification\n");
        return 1;
    }

    FILE *in = stdin;
    if (file_arg) {
        in = fopen(file_arg, "rb");
        if (!in) {
            perror(file_arg);
            return 1;
        }
    }

    unsigned long id = next_job_id();
    char sp[256];
    char mp[256];
    (void)snprintf(sp, sizeof(sp), "%s/job.%lu.sh", AT_SPOOL_DIR, id);
    (void)snprintf(mp, sizeof(mp), "%s/job.%lu.meta", AT_SPOOL_DIR, id);

    FILE *sfp = fopen(sp, "wb");
    if (!sfp) {
        perror(sp);
        if (in != stdin) {
            (void)fclose(in);
        }
        return 1;
    }
    char buf[512];
    size_t nr = 0;
    while ((nr = fread(buf, 1, sizeof(buf), in)) > 0) {
        (void)fwrite(buf, 1, nr, sfp);
    }
    (void)fclose(sfp);
    if (in != stdin) {
        (void)fclose(in);
    }

    char tstr[64];
    struct tm tm_buf;
    if (gmtime_r(&when, &tm_buf)) {
        (void)strftime(tstr, sizeof(tstr), "%Y-%m-%d %H:%M:%S", &tm_buf);
    } else {
        (void)snprintf(tstr, sizeof(tstr), "1970-01-01 00:00:00");
    }

    FILE *mfp = fopen(mp, "wb");
    if (!mfp) {
        perror(mp);
        (void)unlink(sp);
        return 1;
    }
    (void)fprintf(mfp, "id=%lu\nqueue=%c\nmail=%d\ntime=%s\n", id, queue,
                  send_mail, tstr);
    (void)fclose(mfp);

    (void)fprintf(stderr, "job %lu at %s\n", id, tstr);
    return 0;
}
