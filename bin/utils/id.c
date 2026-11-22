/**
 * @file     id.c
 * @brief    POSIX.1-2024 XCU id utility (return user and group identity)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* need:: COMP_PROC_0010 */

static void print_uid_name(uid_t uid, int as_name) {
    if (as_name) {
        struct passwd *pw = getpwuid(uid);
        if (pw) {
            (void)printf("%s\n", pw->pw_name);
            return;
        }
    }
    (void)printf("%u\n", (unsigned)uid);
}

static void print_gid_name(gid_t gid, int as_name) {
    if (as_name) {
        struct group *gr = getgrgid(gid);
        if (gr) {
            (void)printf("%s\n", gr->gr_name);
            return;
        }
    }
    (void)printf("%u\n", (unsigned)gid);
}

int main(int argc, char **argv) {
    int opt_G = 0;
    int opt_g = 0;
    int opt_u = 0;
    int opt_n = 0;
    int opt_r = 0;

    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "Ggunr")) != -1) {
        switch (c) {
        case 'G':
            opt_G = 1;
            break;
        case 'g':
            opt_g = 1;
            break;
        case 'u':
            opt_u = 1;
            break;
        case 'n':
            opt_n = 1;
            break;
        case 'r':
            opt_r = 1;
            break;
        default:
            (void)fprintf(stderr, "id: invalid option -%c\n", optopt);
            return 1;
        }
    }

    int mode_count = opt_G + opt_g + opt_u;
    if (mode_count > 1) {
        (void)fprintf(stderr, "id: cannot combine -G, -g, and -u\n");
        return 1;
    }
    if ((opt_n || opt_r) && mode_count == 0) {
        (void)fprintf(stderr, "id: -n and -r require -G, -g, or -u\n");
        return 1;
    }
    if (argc - optind > 1) {
        (void)fprintf(stderr, "id: too many operands\n");
        return 1;
    }

    uid_t ruid = getuid();
    uid_t euid = geteuid();
    gid_t rgid = getgid();
    gid_t egid = getegid();
    gid_t groups[16];
    int ngroups = 0;

    if (optind < argc) {
        struct passwd *pw = getpwnam(argv[optind]);
        if (!pw) {
            (void)fprintf(stderr, "id: no such user: %s\n", argv[optind]);
            return 1;
        }
        ruid = pw->pw_uid;
        euid = pw->pw_uid;
        rgid = pw->pw_gid;
        egid = pw->pw_gid;
        groups[0] = pw->pw_gid;
        ngroups = 1;
    } else {
        ngroups = getgroups(16, groups);
        if (ngroups <= 0) {
            groups[0] = egid;
            ngroups = 1;
        }
    }

    if (opt_u) {
        print_uid_name(opt_r ? ruid : euid, opt_n);
        return 0;
    }
    if (opt_g) {
        print_gid_name(opt_r ? rgid : egid, opt_n);
        return 0;
    }
    if (opt_G) {
        for (int i = 0; i < ngroups; i++) {
            if (i > 0) {
                (void)putchar(' ');
            }
            if (opt_n) {
                struct group *gr = getgrgid(groups[i]);
                if (gr) {
                    (void)fputs(gr->gr_name, stdout);
                    continue;
                }
            }
            (void)printf("%u", (unsigned)groups[i]);
        }
        (void)putchar('\n');
        return 0;
    }

    struct passwd *pw = getpwuid(ruid);
    struct group *gr = getgrgid(rgid);
    (void)printf("uid=%u", (unsigned)ruid);
    if (pw) {
        (void)printf("(%s)", pw->pw_name);
    }
    (void)printf(" gid=%u", (unsigned)rgid);
    if (gr) {
        (void)printf("(%s)", gr->gr_name);
    }
    if (euid != ruid) {
        struct passwd *epw = getpwuid(euid);
        (void)printf(" euid=%u", (unsigned)euid);
        if (epw) {
            (void)printf("(%s)", epw->pw_name);
        }
    }
    if (egid != rgid) {
        struct group *egr = getgrgid(egid);
        (void)printf(" egid=%u", (unsigned)egid);
        if (egr) {
            (void)printf("(%s)", egr->gr_name);
        }
    }
    (void)fputs(" groups=", stdout);
    for (int i = 0; i < ngroups; i++) {
        if (i > 0) {
            (void)putchar(',');
        }
        struct group *g = getgrgid(groups[i]);
        (void)printf("%u", (unsigned)groups[i]);
        if (g) {
            (void)printf("(%s)", g->gr_name);
        }
    }
    (void)putchar('\n');
    return 0;
}
