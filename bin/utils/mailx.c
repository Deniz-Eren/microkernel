/**
 * @file     mailx.c
 * @brief    Ring 3 POSIX.1-2024 XCU mailx electronic mail utility
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
#include <time.h>
#include <unistd.h>

/* need:: COMP_LIBC_0027 */

#define MAIL_SPOOL_DIR "/tmp/mail"

typedef struct {
    char *raw;
    size_t len;
    char from[128];
    char date[64];
    char subject[256];
    int deleted;
} mail_msg_t;

typedef struct {
    mail_msg_t *items;
    size_t len;
    size_t cap;
} mbox_t;

static void mbox_free(mbox_t *mb) {
    for (size_t i = 0; i < mb->len; i++) {
        free(mb->items[i].raw);
    }
    free(mb->items);
    mb->items = NULL;
    mb->len = 0;
    mb->cap = 0;
}

static const char *current_user(void) {
    const char *u = getlogin();
    if (!u || u[0] == '\0') {
        struct passwd *pw = getpwuid(getuid());
        u = (pw && pw->pw_name) ? pw->pw_name : "root";
    }
    return u;
}

static void format_date(char *out, size_t cap) {
    time_t now = time(NULL);
    struct tm tm_buf;
    if (gmtime_r(&now, &tm_buf)) {
        (void)strftime(out, cap, "%a %b %d %H:%M:%S %Y", &tm_buf);
    } else {
        (void)snprintf(out, cap, "Thu Jan 01 00:00:00 1970");
    }
}

static int ensure_mail_dir(void) {
    if (mkdir(MAIL_SPOOL_DIR, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

static int send_mail(const char *subject, const char *from_hdr, int argc,
                     char **argv, int start) {
    if (ensure_mail_dir() != 0) {
        perror("mailx: spool");
        return 1;
    }
    size_t cap = 512;
    size_t len = 0;
    char *body = malloc(cap);
    if (!body) {
        return 1;
    }
    char line[512];
    while (fgets(line, sizeof(line), stdin) != NULL) {
        if (strcmp(line, "~.\n") == 0 || strcmp(line, "~.") == 0) {
            break;
        }
        size_t ll = strlen(line);
        if (len + ll + 1 > cap) {
            while (len + ll + 1 > cap) {
                cap *= 2;
            }
            char *nb = realloc(body, cap);
            if (!nb) {
                free(body);
                return 1;
            }
            body = nb;
        }
        (void)memcpy(body + len, line, ll);
        len += ll;
    }
    body[len] = '\0';

    const char *sender =
        (from_hdr && from_hdr[0] != '\0') ? from_hdr : current_user();
    char date[64];
    format_date(date, sizeof(date));

    int rc = 0;
    for (int i = start; i < argc; i++) {
        char mpath[256];
        if (strchr(argv[i], '/') != NULL) {
            (void)snprintf(mpath, sizeof(mpath), "%s", argv[i]);
        } else {
            (void)snprintf(mpath, sizeof(mpath), "%s/%s", MAIL_SPOOL_DIR,
                           argv[i]);
        }
        FILE *fp = fopen(mpath, "ab");
        if (!fp) {
            perror(mpath);
            rc = 1;
            continue;
        }
        (void)fprintf(fp, "From %s %s\n", sender, date);
        (void)fprintf(fp, "From: %s\n", sender);
        (void)fprintf(fp, "To: %s\n", argv[i]);
        if (subject && subject[0] != '\0') {
            (void)fprintf(fp, "Subject: %s\n", subject);
        }
        (void)fprintf(fp, "Date: %s\n\n", date);
        if (len > 0) {
            (void)fwrite(body, 1, len, fp);
            if (body[len - 1] != '\n') {
                (void)fputc('\n', fp);
            }
        }
        (void)fputc('\n', fp);
        (void)fclose(fp);
    }
    free(body);
    return rc;
}

static void parse_msg_headers(mail_msg_t *m) {
    m->from[0] = '\0';
    m->date[0] = '\0';
    m->subject[0] = '\0';
    const char *p = m->raw ? m->raw : "";
    while (*p != '\0') {
        const char *nl = strchr(p, '\n');
        size_t ll = nl ? (size_t)(nl - p) : strlen(p);
        if (ll == 0) {
            break;
        }
        if (strncmp(p, "From: ", 6) == 0) {
            size_t n = (ll - 6 < sizeof(m->from) - 1) ? (ll - 6)
                                                      : (sizeof(m->from) - 1);
            (void)memcpy(m->from, p + 6, n);
            m->from[n] = '\0';
        } else if (strncmp(p, "Subject: ", 9) == 0) {
            size_t n = (ll - 9 < sizeof(m->subject) - 1)
                           ? (ll - 9)
                           : (sizeof(m->subject) - 1);
            (void)memcpy(m->subject, p + 9, n);
            m->subject[n] = '\0';
        } else if (strncmp(p, "Date: ", 6) == 0) {
            size_t n = (ll - 6 < sizeof(m->date) - 1) ? (ll - 6)
                                                      : (sizeof(m->date) - 1);
            (void)memcpy(m->date, p + 6, n);
            m->date[n] = '\0';
        }
        if (!nl) {
            break;
        }
        p = nl + 1;
    }
    if (m->from[0] == '\0') {
        (void)snprintf(m->from, sizeof(m->from), "unknown");
    }
}

static int load_mbox(const char *path, mbox_t *mb) {
    (void)memset(mb, 0, sizeof(*mb));
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return 0;
    }
    char line[512];
    size_t cur_cap = 0;
    mail_msg_t *cur = NULL;
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "From ", 5) == 0) {
            if (cur) {
                parse_msg_headers(cur);
            }
            if (mb->len == mb->cap) {
                size_t nc = (mb->cap == 0) ? 8 : mb->cap * 2;
                mail_msg_t *ni = realloc(mb->items, nc * sizeof(*ni));
                if (!ni) {
                    (void)fclose(fp);
                    return -1;
                }
                (void)memset(ni + mb->cap, 0, (nc - mb->cap) * sizeof(*ni));
                mb->items = ni;
                mb->cap = nc;
            }
            cur = &mb->items[mb->len++];
            cur_cap = 512;
            cur->raw = calloc(1, cur_cap);
            cur->len = 0;
        }
        if (!cur || !cur->raw) {
            continue;
        }
        size_t ll = strlen(line);
        if (cur->len + ll + 1 > cur_cap) {
            while (cur->len + ll + 1 > cur_cap) {
                cur_cap *= 2;
            }
            char *nr = realloc(cur->raw, cur_cap);
            if (!nr) {
                (void)fclose(fp);
                return -1;
            }
            cur->raw = nr;
        }
        (void)memcpy(cur->raw + cur->len, line, ll);
        cur->len += ll;
        cur->raw[cur->len] = '\0';
    }
    if (cur) {
        parse_msg_headers(cur);
    }
    (void)fclose(fp);
    return 0;
}

static int save_mbox(const char *path, const mbox_t *mb) {
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        perror(path);
        return -1;
    }
    for (size_t i = 0; i < mb->len; i++) {
        if (!mb->items[i].deleted && mb->items[i].raw) {
            (void)fwrite(mb->items[i].raw, 1, mb->items[i].len, fp);
        }
    }
    (void)fclose(fp);
    return 0;
}

static void print_headers(const mbox_t *mb, size_t cur_idx) {
    for (size_t i = 0; i < mb->len; i++) {
        if (mb->items[i].deleted) {
            continue;
        }
        (void)printf("%c%3zu %-12s %s \"%s\"\n", (i == cur_idx) ? '>' : ' ',
                     i + 1, mb->items[i].from, mb->items[i].date,
                     mb->items[i].subject);
    }
}

int main(int argc, char **argv) {
    int opt_e = 0;
    int opt_H = 0;
    int opt_N = 0;
    int opt_f = 0;
    const char *file_arg = NULL;
    const char *user_arg = NULL;
    const char *subj_arg = NULL;
    const char *from_arg = NULL;
    int opt = 0;

    while ((opt = getopt(argc, argv, "efFHinNs:u:r:c:b:")) != -1) {
        switch (opt) {
        case 'e':
            opt_e = 1;
            break;
        case 'f':
            opt_f = 1;
            break;
        case 'H':
            opt_H = 1;
            break;
        case 'N':
            opt_N = 1;
            break;
        case 'F':
        case 'i':
        case 'n':
        case 'c':
        case 'b':
            break;
        case 's':
            subj_arg = optarg;
            break;
        case 'u':
            user_arg = optarg;
            break;
        case 'r':
            from_arg = optarg;
            break;
        default:
            (void)fprintf(stderr,
                          "usage: mailx [-s subject] addr... | mailx [-eHN] "
                          "[-f [file]] [-u user]\n");
            return 1;
        }
    }

    /* Send mode when recipient addresses are given without receive flags */
    if (!opt_e && !opt_H && !opt_f && !user_arg && optind < argc) {
        return send_mail(subj_arg, from_arg, argc, argv, optind);
    }

    char mpath[256];
    if (opt_f && optind < argc) {
        file_arg = argv[optind];
    }
    if (file_arg) {
        (void)snprintf(mpath, sizeof(mpath), "%s", file_arg);
    } else if (user_arg) {
        (void)snprintf(mpath, sizeof(mpath), "%s/%s", MAIL_SPOOL_DIR, user_arg);
    } else {
        const char *env_mail = getenv("MAIL");
        if (env_mail && env_mail[0] != '\0') {
            (void)snprintf(mpath, sizeof(mpath), "%s", env_mail);
        } else {
            (void)snprintf(mpath, sizeof(mpath), "%s/%s", MAIL_SPOOL_DIR,
                           current_user());
        }
    }

    mbox_t mb;
    if (load_mbox(mpath, &mb) != 0) {
        mbox_free(&mb);
        return 1;
    }

    if (opt_e) {
        int rc = (mb.len > 0) ? 0 : 1;
        mbox_free(&mb);
        return rc;
    }
    if (mb.len == 0) {
        (void)printf("No mail for %s\n", user_arg ? user_arg : current_user());
        mbox_free(&mb);
        return 0;
    }
    if (opt_H) {
        print_headers(&mb, 0);
        mbox_free(&mb);
        return 0;
    }

    size_t cur = 0;
    if (!opt_N) {
        print_headers(&mb, cur);
    }

    int modified = 0;
    char cmdline[256];
    while (fgets(cmdline, sizeof(cmdline), stdin) != NULL) {
        char *p = cmdline;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        size_t cl = strlen(p);
        while (cl > 0 && (p[cl - 1] == '\n' || p[cl - 1] == '\r')) {
            p[--cl] = '\0';
        }
        if (cl == 0 || strcmp(p, "p") == 0 || strcmp(p, "t") == 0 ||
            strncmp(p, "p ", 2) == 0 || strncmp(p, "t ", 2) == 0 ||
            isdigit((unsigned char)p[0])) {
            size_t idx = cur;
            if (isdigit((unsigned char)p[0])) {
                idx = strtoul(p, NULL, 10) - 1;
            } else if ((p[0] == 'p' || p[0] == 't') && p[1] == ' ') {
                idx = strtoul(p + 2, NULL, 10) - 1;
            }
            if (idx < mb.len && !mb.items[idx].deleted) {
                cur = idx;
                (void)printf("Message %zu:\n%s", idx + 1, mb.items[idx].raw);
            }
        } else if (strcmp(p, "h") == 0 || strcmp(p, "headers") == 0) {
            print_headers(&mb, cur);
        } else if (strcmp(p, "=") == 0) {
            (void)printf("%zu\n", cur + 1);
        } else if (p[0] == 'd' && (p[1] == '\0' || p[1] == ' ')) {
            size_t idx = (p[1] == ' ') ? (strtoul(p + 2, NULL, 10) - 1) : cur;
            if (idx < mb.len) {
                mb.items[idx].deleted = 1;
                modified = 1;
            }
        } else if (p[0] == 'u' && (p[1] == '\0' || p[1] == ' ')) {
            size_t idx = (p[1] == ' ') ? (strtoul(p + 2, NULL, 10) - 1) : cur;
            if (idx < mb.len) {
                mb.items[idx].deleted = 0;
                modified = 1;
            }
        } else if (strcmp(p, "q") == 0 || strcmp(p, "quit") == 0) {
            if (modified) {
                (void)save_mbox(mpath, &mb);
            }
            break;
        } else if (strcmp(p, "x") == 0 || strcmp(p, "exit") == 0) {
            break;
        }
    }

    mbox_free(&mb);
    return 0;
}
