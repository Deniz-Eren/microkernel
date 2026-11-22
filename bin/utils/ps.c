/**
 * @file     ps.c
 * @brief    POSIX.1-2024 XCU ps utility (report process status)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fd.h>
#include <sys/proc.h>
#include <unistd.h>

/* need:: COMP_PROC_0009 COMP_PROC_0010 */

#define PS_MAX_FILTERS 32u
#define PS_MAX_COLS 16u

typedef enum {
    COL_PID,
    COL_PPID,
    COL_PGID,
    COL_RUSER,
    COL_USER,
    COL_RGROUP,
    COL_GROUP,
    COL_COMM,
    COL_ARGS,
    COL_TIME,
    COL_ETIME,
    COL_TTY,
    COL_NICE,
    COL_PCPU,
    COL_VSZ,
    COL_UID_NUM,
    COL_C,
    COL_STIME,
    COL_F,
    COL_S,
    COL_PRI
} ps_col_id_t;

typedef struct {
    ps_col_id_t id;
    char header[24];
    int right_align;
    int width;
} ps_col_t;

typedef struct {
    const char *name;
    ps_col_id_t id;
    const char *def_hdr;
    int right_align;
    int width;
} ps_col_desc_t;

static const ps_col_desc_t k_col_descs[] = {
    {"pid", COL_PID, "PID", 1, 5},
    {"ppid", COL_PPID, "PPID", 1, 5},
    {"pgid", COL_PGID, "PGID", 1, 5},
    {"ruser", COL_RUSER, "RUSER", 0, 8},
    {"user", COL_USER, "USER", 0, 8},
    {"rgroup", COL_RGROUP, "RGROUP", 0, 8},
    {"group", COL_GROUP, "GROUP", 0, 8},
    {"comm", COL_COMM, "COMMAND", 0, 0},
    {"args", COL_ARGS, "COMMAND", 0, 0},
    {"time", COL_TIME, "TIME", 1, 8},
    {"etime", COL_ETIME, "ELAPSED", 1, 8},
    {"tty", COL_TTY, "TTY", 0, 7},
    {"nice", COL_NICE, "NI", 1, 2},
    {"pcpu", COL_PCPU, "%CPU", 1, 4},
    {"vsz", COL_VSZ, "VSZ", 1, 5},
};

static int parse_id_list(const char *arg, uint32_t *out, size_t *count) {
    const char *p = arg;
    int added = 0;
    while (*p) {
        while (*p == ',' || isspace((unsigned char)*p)) {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        char *end = NULL;
        errno = 0;
        unsigned long val = strtoul(p, &end, 10);
        if (errno != 0 || end == p ||
            (*end != '\0' && *end != ',' && !isspace((unsigned char)*end))) {
            return -1;
        }
        if (*count < PS_MAX_FILTERS) {
            out[(*count)++] = (uint32_t)val;
        }
        added = 1;
        p = end;
    }
    return added ? 0 : -1;
}

static int add_col(ps_col_t *cols, size_t *ncols, ps_col_id_t id,
                   const char *hdr, int right_align, int width) {
    if (*ncols >= PS_MAX_COLS) {
        return -1;
    }
    ps_col_t *c = &cols[(*ncols)++];
    c->id = id;
    c->right_align = right_align;
    c->width = width;
    (void)strlcpy(c->header, hdr, sizeof(c->header));
    return 0;
}

static int parse_o_spec(const char *arg, ps_col_t *cols, size_t *ncols) {
    const char *p = arg;
    int added = 0;
    while (*p) {
        while (*p == ',' || isspace((unsigned char)*p)) {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        const char *start = p;
        while (*p && *p != ',' && !isspace((unsigned char)*p) && *p != '=') {
            p++;
        }
        size_t klen = (size_t)(p - start);
        char key[24];
        if (klen == 0u || klen >= sizeof(key)) {
            return -1;
        }
        memcpy(key, start, klen);
        key[klen] = '\0';

        const ps_col_desc_t *desc = NULL;
        for (size_t i = 0; i < sizeof(k_col_descs) / sizeof(k_col_descs[0]);
             i++) {
            if (strcmp(k_col_descs[i].name, key) == 0) {
                desc = &k_col_descs[i];
                break;
            }
        }
        if (!desc) {
            return -1;
        }

        char hdr[24];
        if (*p == '=') {
            p++;
            const char *hstart = p;
            while (*p && *p != ',') {
                p++;
            }
            size_t hlen = (size_t)(p - hstart);
            if (hlen >= sizeof(hdr)) {
                hlen = sizeof(hdr) - 1u;
            }
            memcpy(hdr, hstart, hlen);
            hdr[hlen] = '\0';
            if (add_col(cols, ncols, desc->id, hdr, desc->right_align,
                        desc->width) != 0) {
                return -1;
            }
            added = 1;
            continue;
        }
        (void)strlcpy(hdr, desc->def_hdr, sizeof(hdr));
        if (add_col(cols, ncols, desc->id, hdr, desc->right_align,
                    desc->width) != 0) {
            return -1;
        }
        added = 1;
    }
    return added ? 0 : -1;
}

static int id_in_list(uint32_t id, const uint32_t *list, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (list[i] == id) {
            return 1;
        }
    }
    return 0;
}

static const char *tty_str(uint32_t kind) {
    if (kind == FD_KIND_TTY) {
        return "tty";
    }
    if (kind == FD_KIND_CONSOLE) {
        return "console";
    }
    return "-";
}

static void format_time_str(uint64_t ticks_ms, char *buf, size_t cap) {
    uint64_t secs = ticks_ms / 1000ULL;
    uint64_t mins = (secs / 60ULL) % 60ULL;
    uint64_t hrs = secs / 3600ULL;
    uint64_t rem_s = secs % 60ULL;
    (void)snprintf(buf, cap, "%02lu:%02lu:%02lu", (unsigned long)hrs,
                   (unsigned long)mins, (unsigned long)rem_s);
}

static void format_col_value(const struct __k_proc_info *pi, ps_col_id_t id,
                             char *buf, size_t cap) {
    switch (id) {
    case COL_PID:
        (void)snprintf(buf, cap, "%u", pi->pid);
        break;
    case COL_PPID:
        (void)snprintf(buf, cap, "%u", pi->ppid);
        break;
    case COL_PGID:
        (void)snprintf(buf, cap, "%u", pi->pgid);
        break;
    case COL_RUSER:
    case COL_USER: {
        struct passwd *pw = getpwuid(pi->uid);
        if (pw) {
            (void)strlcpy(buf, pw->pw_name, cap);
        } else {
            (void)snprintf(buf, cap, "%u", pi->uid);
        }
        break;
    }
    case COL_RGROUP:
    case COL_GROUP: {
        struct group *gr = getgrgid(pi->gid);
        if (gr) {
            (void)strlcpy(buf, gr->gr_name, cap);
        } else {
            (void)snprintf(buf, cap, "%u", pi->gid);
        }
        break;
    }
    case COL_UID_NUM:
        (void)snprintf(buf, cap, "%u", pi->uid);
        break;
    case COL_NICE:
        (void)snprintf(buf, cap, "%d", pi->nice);
        break;
    case COL_C:
    case COL_F:
        (void)strlcpy(buf, "0", cap);
        break;
    case COL_STIME:
        (void)strlcpy(buf, "00:00", cap);
        break;
    case COL_S:
        (void)strlcpy(buf, pi->state == PROC_INFO_ZOMBIE ? "Z" : "R", cap);
        break;
    case COL_PRI:
        (void)snprintf(buf, cap, "%u", pi->prio);
        break;
    case COL_TTY:
        (void)strlcpy(buf, tty_str(pi->tty_kind), cap);
        break;
    case COL_TIME:
        format_time_str(pi->utime_ticks + pi->stime_ticks, buf, cap);
        break;
    case COL_ETIME:
        format_time_str(pi->utime_ticks + pi->stime_ticks, buf, cap);
        break;
    case COL_PCPU:
        (void)strlcpy(buf, "0.0", cap);
        break;
    case COL_VSZ:
        (void)snprintf(buf, cap, "%u", pi->nthreads * 64u);
        break;
    case COL_COMM:
        (void)strlcpy(buf, pi->comm[0] ? pi->comm : "-", cap);
        break;
    case COL_ARGS:
        (void)strlcpy(buf, pi->args[0] ? pi->args : pi->comm, cap);
        break;
    }
}

static void print_cell(const char *val, int width, int right_align,
                       int is_last) {
    if (is_last && width == 0) {
        (void)fputs(val, stdout);
        return;
    }
    size_t len = strlen(val);
    int pad = (width > (int)len) ? (width - (int)len) : 0;
    if (right_align) {
        for (int i = 0; i < pad; i++) {
            (void)putchar(' ');
        }
        (void)fputs(val, stdout);
    } else {
        (void)fputs(val, stdout);
        if (!is_last) {
            for (int i = 0; i < pad; i++) {
                (void)putchar(' ');
            }
        }
    }
}

int main(int argc, char **argv) {
    int opt_all = 0;
    int opt_a = 0;
    int opt_d = 0;
    int opt_f = 0;
    int opt_l = 0;
    int has_o = 0;

    uint32_t pid_filter[PS_MAX_FILTERS];
    size_t pid_filter_n = 0u;
    uint32_t grp_filter[PS_MAX_FILTERS];
    size_t grp_filter_n = 0u;
    ps_col_t cols[PS_MAX_COLS];
    size_t ncols = 0u;

    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "Aadeflp:g:o:")) != -1) {
        switch (c) {
        case 'A':
        case 'e':
            opt_all = 1;
            break;
        case 'a':
            opt_a = 1;
            break;
        case 'd':
            opt_d = 1;
            break;
        case 'f':
            opt_f = 1;
            break;
        case 'l':
            opt_l = 1;
            break;
        case 'p':
            if (parse_id_list(optarg, pid_filter, &pid_filter_n) != 0) {
                (void)fprintf(stderr, "ps: invalid pid list: %s\n", optarg);
                return 1;
            }
            break;
        case 'g':
            if (parse_id_list(optarg, grp_filter, &grp_filter_n) != 0) {
                (void)fprintf(stderr, "ps: invalid group list: %s\n", optarg);
                return 1;
            }
            break;
        case 'o':
            has_o = 1;
            if (parse_o_spec(optarg, cols, &ncols) != 0) {
                (void)fprintf(stderr, "ps: invalid -o format: %s\n", optarg);
                return 1;
            }
            break;
        default:
            (void)fprintf(stderr, "ps: invalid option -%c\n", optopt);
            return 1;
        }
    }
    if (optind < argc) {
        (void)fprintf(stderr, "ps: unexpected operand: %s\n", argv[optind]);
        return 1;
    }

    if (!has_o) {
        if (opt_l) {
            (void)add_col(cols, &ncols, COL_F, "F", 1, 1);
            (void)add_col(cols, &ncols, COL_S, "S", 0, 1);
            (void)add_col(cols, &ncols, COL_UID_NUM, "UID", 1, 4);
            (void)add_col(cols, &ncols, COL_PID, "PID", 1, 5);
            (void)add_col(cols, &ncols, COL_PPID, "PPID", 1, 5);
            (void)add_col(cols, &ncols, COL_C, "C", 1, 2);
            (void)add_col(cols, &ncols, COL_PRI, "PRI", 1, 3);
            (void)add_col(cols, &ncols, COL_NICE, "NI", 1, 2);
            (void)add_col(cols, &ncols, COL_TTY, "TTY", 0, 7);
            (void)add_col(cols, &ncols, COL_TIME, "TIME", 1, 8);
            (void)add_col(cols, &ncols, COL_COMM, "CMD", 0, 0);
        } else if (opt_f) {
            (void)add_col(cols, &ncols, COL_USER, "UID", 0, 6);
            (void)add_col(cols, &ncols, COL_PID, "PID", 1, 5);
            (void)add_col(cols, &ncols, COL_PPID, "PPID", 1, 5);
            (void)add_col(cols, &ncols, COL_C, "C", 1, 2);
            (void)add_col(cols, &ncols, COL_STIME, "STIME", 0, 5);
            (void)add_col(cols, &ncols, COL_TTY, "TTY", 0, 7);
            (void)add_col(cols, &ncols, COL_TIME, "TIME", 1, 8);
            (void)add_col(cols, &ncols, COL_ARGS, "CMD", 0, 0);
        } else {
            (void)add_col(cols, &ncols, COL_PID, "PID", 1, 5);
            (void)add_col(cols, &ncols, COL_TTY, "TTY", 0, 7);
            (void)add_col(cols, &ncols, COL_TIME, "TIME", 1, 8);
            (void)add_col(cols, &ncols, COL_COMM, "CMD", 0, 0);
        }
    }

    struct __k_proc_info list[PROC_LIST_MAX];
    memset(list, 0, sizeof(list));
    long n = ProcList(list, PROC_LIST_MAX);
    if (n < 0) {
        (void)fprintf(stderr, "ps: failed to list processes\n");
        return 1;
    }

    int show_header = 0;
    for (size_t i = 0; i < ncols; i++) {
        if (cols[i].header[0] != '\0') {
            show_header = 1;
            break;
        }
    }
    if (show_header) {
        for (size_t i = 0; i < ncols; i++) {
            if (i > 0u) {
                (void)putchar(' ');
            }
            print_cell(cols[i].header, cols[i].width, cols[i].right_align,
                       i + 1u == ncols);
        }
        (void)putchar('\n');
    }

    pid_t my_sid = getsid(0);
    int explicit_sel =
        opt_all || opt_a || opt_d || pid_filter_n > 0u || grp_filter_n > 0u;

    for (long i = 0; i < n; i++) {
        const struct __k_proc_info *pi = &list[i];
        int match = 0;
        if (!explicit_sel) {
            match = (pi->tty_kind != FD_KIND_NONE) &&
                    ((pid_t)pi->sid == my_sid || my_sid <= 0);
        } else {
            if (opt_all) {
                match = 1;
            }
            if (opt_d && pi->pid != pi->sid) {
                match = 1;
            }
            if (opt_a && pi->tty_kind != FD_KIND_NONE && pi->pid != pi->sid) {
                match = 1;
            }
            if (pid_filter_n > 0u &&
                id_in_list(pi->pid, pid_filter, pid_filter_n)) {
                match = 1;
            }
            if (grp_filter_n > 0u &&
                (id_in_list(pi->sid, grp_filter, grp_filter_n) ||
                 id_in_list(pi->pgid, grp_filter, grp_filter_n))) {
                match = 1;
            }
        }
        if (!match) {
            continue;
        }

        for (size_t col = 0; col < ncols; col++) {
            if (col > 0u) {
                (void)putchar(' ');
            }
            char cell[PROC_ARGS_MAX + 8u];
            format_col_value(pi, cols[col].id, cell, sizeof(cell));
            print_cell(cell, cols[col].width, cols[col].right_align,
                       col + 1u == ncols);
        }
        (void)putchar('\n');
    }

    return 0;
}
