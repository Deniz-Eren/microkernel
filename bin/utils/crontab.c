/**
 * @file     crontab.c
 * @brief    Ring 3 POSIX.1-2024 XCU crontab scheduled-job table utility
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* need:: COMP_LIBC_0027 */

#define CRON_SPOOL_DIR "/tmp/spool/cron"

static int ensure_cron_dir(void) {
    if (mkdir("/tmp/spool", 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    if (mkdir(CRON_SPOOL_DIR, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

static void get_cron_path(char *out, size_t cap) {
    const char *user = getlogin();
    if (!user || user[0] == '\0') {
        struct passwd *pw = getpwuid(getuid());
        user = (pw && pw->pw_name) ? pw->pw_name : "root";
    }
    (void)snprintf(out, cap, "%s/%s", CRON_SPOOL_DIR, user);
}

static int valid_field_token(const char *tok, int lo, int hi) {
    if (strcmp(tok, "*") == 0) {
        return 1;
    }
    const char *p = tok;
    while (*p != '\0') {
        if (*p == '*') {
            p++;
            if (*p == '/') {
                p++;
                if (!isdigit((unsigned char)*p)) {
                    return 0;
                }
                while (isdigit((unsigned char)*p)) {
                    p++;
                }
            }
        } else if (isdigit((unsigned char)*p)) {
            char *end = NULL;
            long a = strtol(p, &end, 10);
            if (a < lo || a > hi) {
                return 0;
            }
            p = end;
            if (*p == '-') {
                p++;
                if (!isdigit((unsigned char)*p)) {
                    return 0;
                }
                long b = strtol(p, &end, 10);
                if (b < lo || b > hi || b < a) {
                    return 0;
                }
                p = end;
            }
            if (*p == '/') {
                p++;
                if (!isdigit((unsigned char)*p)) {
                    return 0;
                }
                while (isdigit((unsigned char)*p)) {
                    p++;
                }
            }
        } else {
            return 0;
        }
        if (*p == ',') {
            p++;
            if (*p == '\0') {
                return 0;
            }
        } else if (*p != '\0') {
            return 0;
        }
    }
    return 1;
}

static int validate_cron_line(const char *line) {
    const char *p = line;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p == '\0' || *p == '#' || *p == '\n') {
        return 0;
    }
    static const int k_lo[5] = {0, 0, 1, 1, 0};
    static const int k_hi[5] = {59, 23, 31, 12, 7};
    for (int f = 0; f < 5; f++) {
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            return -1;
        }
        char tok[64];
        size_t n = 0;
        while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n' &&
               n + 1 < sizeof(tok)) {
            tok[n++] = *p++;
        }
        tok[n] = '\0';
        if (!valid_field_token(tok, k_lo[f], k_hi[f])) {
            return -1;
        }
    }
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return (*p != '\0' && *p != '\n') ? 0 : -1;
}

static int install_crontab(FILE *in, const char *dst_path) {
    size_t cap = 512;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        return 1;
    }
    char line[512];
    int lno = 0;
    while (fgets(line, sizeof(line), in) != NULL) {
        lno++;
        if (validate_cron_line(line) != 0) {
            (void)fprintf(stderr, "crontab: syntax error at line %d\n", lno);
            free(buf);
            return 1;
        }
        size_t ll = strlen(line);
        if (len + ll + 1 > cap) {
            while (len + ll + 1 > cap) {
                cap *= 2;
            }
            char *nb = realloc(buf, cap);
            if (!nb) {
                free(buf);
                return 1;
            }
            buf = nb;
        }
        (void)memcpy(buf + len, line, ll);
        len += ll;
    }
    buf[len] = '\0';

    FILE *out = fopen(dst_path, "wb");
    if (!out) {
        perror(dst_path);
        free(buf);
        return 1;
    }
    if (len > 0) {
        (void)fwrite(buf, 1, len, out);
    }
    (void)fclose(out);
    free(buf);
    return 0;
}

int main(int argc, char **argv) {
    int mode = 0; /* 0=install, 'l'=list, 'r'=remove, 'e'=edit */
    int opt = 0;
    while ((opt = getopt(argc, argv, "elr")) != -1) {
        if (opt == 'e' || opt == 'l' || opt == 'r') {
            if (mode != 0) {
                (void)fprintf(stderr, "usage: crontab [file | -e | -l | -r]\n");
                return 1;
            }
            mode = opt;
        } else {
            (void)fprintf(stderr, "usage: crontab [file | -e | -l | -r]\n");
            return 1;
        }
    }

    if (ensure_cron_dir() != 0) {
        perror("crontab: spool");
        return 1;
    }
    char path[256];
    get_cron_path(path, sizeof(path));

    if (mode == 'l') {
        FILE *fp = fopen(path, "rb");
        if (!fp) {
            (void)fprintf(stderr, "crontab: no crontab for current user\n");
            return 1;
        }
        int ch = 0;
        while ((ch = fgetc(fp)) != EOF) {
            (void)putchar(ch);
        }
        (void)fclose(fp);
        return 0;
    }
    if (mode == 'r') {
        if (unlink(path) != 0 && errno != ENOENT) {
            perror(path);
            return 1;
        }
        return 0;
    }
    if (mode == 'e') {
        char tmp[] = "/tmp/cron_ed_XXXXXX";
        int fd = mkstemp(tmp);
        if (fd < 0) {
            perror("crontab: mkstemp");
            return 1;
        }
        (void)close(fd);
        FILE *existing = fopen(path, "rb");
        if (existing) {
            FILE *tfp = fopen(tmp, "wb");
            if (tfp) {
                int ch = 0;
                while ((ch = fgetc(existing)) != EOF) {
                    (void)fputc(ch, tfp);
                }
                (void)fclose(tfp);
            }
            (void)fclose(existing);
        }
        const char *ed = getenv("VISUAL");
        if (!ed || ed[0] == '\0') {
            ed = getenv("EDITOR");
        }
        if (!ed || ed[0] == '\0') {
            ed = "/boot/ed";
        }
        pid_t pid = fork();
        if (pid == 0) {
            char *av[] = {(char *)ed, tmp, NULL};
            (void)execvp(ed, av);
            _exit(127);
        }
        int st = 0;
        if (pid < 0 || waitpid(pid, &st, 0) < 0 || !WIFEXITED(st) ||
            WEXITSTATUS(st) != 0) {
            (void)unlink(tmp);
            return 1;
        }
        FILE *tfp = fopen(tmp, "rb");
        int rc = tfp ? install_crontab(tfp, path) : 1;
        if (tfp) {
            (void)fclose(tfp);
        }
        (void)unlink(tmp);
        return rc;
    }

    if (argc - optind > 1) {
        (void)fprintf(stderr, "usage: crontab [file | -e | -l | -r]\n");
        return 1;
    }
    FILE *in = stdin;
    if (optind < argc && strcmp(argv[optind], "-") != 0) {
        in = fopen(argv[optind], "rb");
        if (!in) {
            perror(argv[optind]);
            return 1;
        }
    }
    int rc = install_crontab(in, path);
    if (in != stdin) {
        (void)fclose(in);
    }
    return rc;
}
