/**
 * @file     cred.c
 * @brief    POSIX.1-2024 credentials, pwd/grp databases, login, and rlimits
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/microkernel.h>
#include <sys/resource.h>
#include <sys/times.h>
#include <unistd.h>

/* need:: COMP_PROC_0010 COMP_LIBC_0028 */

uid_t getuid(void) {
    return (uid_t)__sys4(SYS_PROC_CRED, CRED_OP_GETUID, 0, 0, 0);
}

uid_t geteuid(void) {
    return (uid_t)__sys4(SYS_PROC_CRED, CRED_OP_GETEUID, 0, 0, 0);
}

int setuid(uid_t uid) {
    long r = __sys4(SYS_PROC_CRED, CRED_OP_SETUID, (long)uid, 0, 0);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int seteuid(uid_t uid) {
    long r = __sys4(SYS_PROC_CRED, CRED_OP_SETEUID, (long)uid, 0, 0);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int setreuid(uid_t ruid, uid_t euid) {
    if (ruid != (uid_t)-1 && setuid(ruid) != 0) {
        return -1;
    }
    if (euid != (uid_t)-1 && seteuid(euid) != 0) {
        return -1;
    }
    return 0;
}

int setresuid(uid_t ruid, uid_t euid, uid_t suid) {
    (void)suid;
    return setreuid(ruid, euid);
}

gid_t getgid(void) {
    return (gid_t)__sys4(SYS_PROC_CRED, CRED_OP_GETGID, 0, 0, 0);
}

gid_t getegid(void) {
    return (gid_t)__sys4(SYS_PROC_CRED, CRED_OP_GETEGID, 0, 0, 0);
}

int setgid(gid_t gid) {
    long r = __sys4(SYS_PROC_CRED, CRED_OP_SETGID, (long)gid, 0, 0);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int setegid(gid_t gid) {
    long r = __sys4(SYS_PROC_CRED, CRED_OP_SETEGID, (long)gid, 0, 0);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int setregid(gid_t rgid, gid_t egid) {
    if (rgid != (gid_t)-1 && setgid(rgid) != 0) {
        return -1;
    }
    if (egid != (gid_t)-1 && setegid(egid) != 0) {
        return -1;
    }
    return 0;
}

int setresgid(gid_t rgid, gid_t egid, gid_t sgid) {
    (void)sgid;
    return setregid(rgid, egid);
}

int getgroups(int gidsetsize, gid_t grouplist[]) {
    long r = __sys4(SYS_PROC_CRED, CRED_OP_GETGROUPS, (long)gidsetsize,
                    (long)(uintptr_t)grouplist, 0);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return (int)r;
}

int setgroups(size_t size, const gid_t *list) {
    if (size > 0u && !list) {
        errno = EFAULT;
        return -1;
    }
    if (geteuid() != 0u) {
        errno = EPERM;
        return -1;
    }
    return 0;
}

int getrlimit(int resource, struct rlimit *rlp) {
    if (!rlp) {
        errno = EFAULT;
        return -1;
    }
    struct __k_rlimit kr = {0, 0};
    long r = __sys3(SYS_PROC_RLIMIT, (long)resource, 0, (long)(uintptr_t)&kr);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    rlp->rlim_cur = (rlim_t)kr.rlim_cur;
    rlp->rlim_max = (rlim_t)kr.rlim_max;
    return 0;
}

int setrlimit(int resource, const struct rlimit *rlp) {
    if (!rlp) {
        errno = EFAULT;
        return -1;
    }
    struct __k_rlimit kr = {.rlim_cur = (uint64_t)rlp->rlim_cur,
                            .rlim_max = (uint64_t)rlp->rlim_max};
    long r = __sys3(SYS_PROC_RLIMIT, (long)resource, (long)(uintptr_t)&kr, 0);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int getrusage(int who, struct rusage *r_usage) {
    if (!r_usage) {
        errno = EFAULT;
        return -1;
    }
    if (who != RUSAGE_SELF && who != RUSAGE_CHILDREN) {
        errno = EINVAL;
        return -1;
    }
    struct tms t = {0};
    if (times(&t) == (clock_t)-1) {
        return -1;
    }
    clock_t ut = (who == RUSAGE_SELF) ? t.tms_utime : t.tms_cutime;
    clock_t st = (who == RUSAGE_SELF) ? t.tms_stime : t.tms_cstime;
    memset(r_usage, 0, sizeof(*r_usage));
    r_usage->ru_utime.tv_sec = (time_t)(ut / 1000L);
    r_usage->ru_utime.tv_usec = (suseconds_t)((ut % 1000L) * 1000L);
    r_usage->ru_stime.tv_sec = (time_t)(st / 1000L);
    r_usage->ru_stime.tv_usec = (suseconds_t)((st % 1000L) * 1000L);
    return 0;
}

typedef struct {
    const char *name;
    const char *passwd;
    uid_t uid;
    gid_t gid;
    const char *gecos;
    const char *dir;
    const char *shell;
} static_pw_t;

static const static_pw_t k_passwd_db[] = {
    {"root", "*", 0u, 0u, "root", "/", "/boot/sh"},
    {"sshd", "*", 22u, 22u, "sshd privilege separation", "/tmp", "/boot/false"},
    {"nobody", "*", 65534u, 65534u, "nobody", "/tmp", "/boot/sh"},
};

typedef struct {
    const char *name;
    gid_t gid;
    const char *members[2];
} static_gr_t;

static const static_gr_t k_group_db[] = {
    {"root", 0u, {"root", NULL}},
    {"wheel", 10u, {"root", NULL}},
    {"sshd", 22u, {"sshd", NULL}},
    {"nogroup", 65534u, {NULL, NULL}},
};

static int fill_passwd(const static_pw_t *src, struct passwd *pwd, char *buffer,
                       size_t bufsize, struct passwd **result) {
    size_t n1 = strlen(src->name) + 1u;
    size_t n2 = strlen(src->passwd) + 1u;
    size_t n3 = strlen(src->gecos) + 1u;
    size_t n4 = strlen(src->dir) + 1u;
    size_t n5 = strlen(src->shell) + 1u;
    if (n1 + n2 + n3 + n4 + n5 > bufsize) {
        *result = NULL;
        return ERANGE;
    }
    char *p = buffer;
    memcpy(p, src->name, n1);
    pwd->pw_name = p;
    p += n1;
    memcpy(p, src->passwd, n2);
    pwd->pw_passwd = p;
    p += n2;
    memcpy(p, src->gecos, n3);
    pwd->pw_gecos = p;
    p += n3;
    memcpy(p, src->dir, n4);
    pwd->pw_dir = p;
    p += n4;
    memcpy(p, src->shell, n5);
    pwd->pw_shell = p;
    pwd->pw_uid = src->uid;
    pwd->pw_gid = src->gid;
    *result = pwd;
    return 0;
}

static int parse_passwd_file(const char *want_name, int by_uid, uid_t want_uid,
                             struct passwd *pwd, char *buffer, size_t bufsize,
                             struct passwd **result) {
    int fd = open("/mnt/etc/passwd", O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return -1;
    }
    char data[2048];
    ssize_t n = read(fd, data, sizeof(data) - 1u);
    (void)close(fd);
    if (n <= 0) {
        return -1;
    }
    data[n] = '\0';

    char *line = data;
    while (*line) {
        char *nl = strchr(line, '\n');
        if (nl) {
            *nl = '\0';
        }
        if (line[0] && line[0] != '#') {
            char *f[7] = {0};
            char *cur = line;
            int nf = 0;
            while (nf < 7) {
                f[nf++] = cur;
                char *col = strchr(cur, ':');
                if (!col) {
                    break;
                }
                *col = '\0';
                cur = col + 1;
            }
            if (nf == 7) {
                uid_t uid = (uid_t)strtoul(f[2], NULL, 10);
                gid_t gid = (gid_t)strtoul(f[3], NULL, 10);
                int match =
                    by_uid ? (uid == want_uid) : (strcmp(f[0], want_name) == 0);
                if (match) {
                    static_pw_t sp = {f[0], f[1], uid, gid, f[4], f[5], f[6]};
                    return fill_passwd(&sp, pwd, buffer, bufsize, result);
                }
            }
        }
        if (!nl) {
            break;
        }
        line = nl + 1;
    }
    return -1;
}

int getpwuid_r(uid_t uid, struct passwd *pwd, char *buffer, size_t bufsize,
               struct passwd **result) {
    if (!pwd || !buffer || !result) {
        if (result) {
            *result = NULL;
        }
        return EINVAL;
    }
    int fr = parse_passwd_file(NULL, 1, uid, pwd, buffer, bufsize, result);
    if (fr >= 0) {
        return fr;
    }
    for (size_t i = 0; i < sizeof(k_passwd_db) / sizeof(k_passwd_db[0]); i++) {
        if (k_passwd_db[i].uid == uid) {
            return fill_passwd(&k_passwd_db[i], pwd, buffer, bufsize, result);
        }
    }
    *result = NULL;
    return 0;
}

int getpwnam_r(const char *name, struct passwd *pwd, char *buffer,
               size_t bufsize, struct passwd **result) {
    if (!name || !pwd || !buffer || !result) {
        if (result) {
            *result = NULL;
        }
        return EINVAL;
    }
    int fr = parse_passwd_file(name, 0, 0u, pwd, buffer, bufsize, result);
    if (fr >= 0) {
        return fr;
    }
    for (size_t i = 0; i < sizeof(k_passwd_db) / sizeof(k_passwd_db[0]); i++) {
        if (strcmp(k_passwd_db[i].name, name) == 0) {
            return fill_passwd(&k_passwd_db[i], pwd, buffer, bufsize, result);
        }
    }
    *result = NULL;
    return 0;
}

struct passwd *getpwuid(uid_t uid) {
    static struct passwd g_pw;
    static char g_pw_buf[256];
    struct passwd *res = NULL;
    int err = getpwuid_r(uid, &g_pw, g_pw_buf, sizeof(g_pw_buf), &res);
    if (err != 0) {
        errno = err;
        return NULL;
    }
    return res;
}

struct passwd *getpwnam(const char *name) {
    static struct passwd g_pw;
    static char g_pw_buf[256];
    struct passwd *res = NULL;
    int err = getpwnam_r(name, &g_pw, g_pw_buf, sizeof(g_pw_buf), &res);
    if (err != 0) {
        errno = err;
        return NULL;
    }
    return res;
}

static size_t g_pwent_idx;

void setpwent(void) {
    g_pwent_idx = 0u;
}

void endpwent(void) {
    g_pwent_idx = 0u;
}

struct passwd *getpwent(void) {
    static struct passwd g_pw;
    static char g_pw_buf[256];
    if (g_pwent_idx >= sizeof(k_passwd_db) / sizeof(k_passwd_db[0])) {
        return NULL;
    }
    struct passwd *res = NULL;
    int err = fill_passwd(&k_passwd_db[g_pwent_idx++], &g_pw, g_pw_buf,
                          sizeof(g_pw_buf), &res);
    if (err != 0) {
        errno = err;
        return NULL;
    }
    return res;
}

static int fill_group(const static_gr_t *src, struct group *grp, char *buffer,
                      size_t bufsize, struct group **result) {
    size_t nmem = 0u;
    size_t str_bytes = strlen(src->name) + 1u;
    while (nmem < 2u && src->members[nmem] != NULL) {
        str_bytes += strlen(src->members[nmem]) + 1u;
        nmem++;
    }
    size_t align_mask = sizeof(char *) - 1u;
    uintptr_t raw = (uintptr_t)buffer;
    uintptr_t aligned = (raw + align_mask) & ~align_mask;
    size_t pad = (size_t)(aligned - raw);
    size_t ptr_bytes = (nmem + 1u) * sizeof(char *);
    if (pad + ptr_bytes + str_bytes > bufsize) {
        *result = NULL;
        return ERANGE;
    }

    char **mem_arr = (char **)aligned;
    char *p = (char *)(aligned + ptr_bytes);
    size_t gname_len = strlen(src->name) + 1u;
    memcpy(p, src->name, gname_len);
    grp->gr_name = p;
    p += gname_len;

    for (size_t i = 0; i < nmem; i++) {
        size_t mlen = strlen(src->members[i]) + 1u;
        memcpy(p, src->members[i], mlen);
        mem_arr[i] = p;
        p += mlen;
    }
    mem_arr[nmem] = NULL;
    grp->gr_gid = src->gid;
    grp->gr_mem = mem_arr;
    *result = grp;
    return 0;
}

int getgrgid_r(gid_t gid, struct group *grp, char *buffer, size_t bufsize,
               struct group **result) {
    if (!grp || !buffer || !result) {
        if (result) {
            *result = NULL;
        }
        return EINVAL;
    }
    for (size_t i = 0; i < sizeof(k_group_db) / sizeof(k_group_db[0]); i++) {
        if (k_group_db[i].gid == gid) {
            return fill_group(&k_group_db[i], grp, buffer, bufsize, result);
        }
    }
    *result = NULL;
    return 0;
}

int getgrnam_r(const char *name, struct group *grp, char *buffer,
               size_t bufsize, struct group **result) {
    if (!name || !grp || !buffer || !result) {
        if (result) {
            *result = NULL;
        }
        return EINVAL;
    }
    for (size_t i = 0; i < sizeof(k_group_db) / sizeof(k_group_db[0]); i++) {
        if (strcmp(k_group_db[i].name, name) == 0) {
            return fill_group(&k_group_db[i], grp, buffer, bufsize, result);
        }
    }
    *result = NULL;
    return 0;
}

struct group *getgrgid(gid_t gid) {
    static struct group g_gr;
    static char g_gr_buf[128];
    struct group *res = NULL;
    int err = getgrgid_r(gid, &g_gr, g_gr_buf, sizeof(g_gr_buf), &res);
    if (err != 0) {
        errno = err;
        return NULL;
    }
    return res;
}

struct group *getgrnam(const char *name) {
    static struct group g_gr;
    static char g_gr_buf[128];
    struct group *res = NULL;
    int err = getgrnam_r(name, &g_gr, g_gr_buf, sizeof(g_gr_buf), &res);
    if (err != 0) {
        errno = err;
        return NULL;
    }
    return res;
}

static size_t g_grent_idx;

void setgrent(void) {
    g_grent_idx = 0u;
}

void endgrent(void) {
    g_grent_idx = 0u;
}

struct group *getgrent(void) {
    static struct group g_gr;
    static char g_gr_buf[128];
    if (g_grent_idx >= sizeof(k_group_db) / sizeof(k_group_db[0])) {
        return NULL;
    }
    struct group *res = NULL;
    int err = fill_group(&k_group_db[g_grent_idx++], &g_gr, g_gr_buf,
                         sizeof(g_gr_buf), &res);
    if (err != 0) {
        errno = err;
        return NULL;
    }
    return res;
}

int getgrouplist(const char *user, gid_t group, gid_t *groups, int *ngroups) {
    if (!user || !ngroups || *ngroups < 0) {
        errno = EINVAL;
        return -1;
    }
    gid_t found[8];
    int count = 0;
    found[count++] = group;
    for (size_t i = 0; i < sizeof(k_group_db) / sizeof(k_group_db[0]); i++) {
        for (size_t m = 0; m < 2u && k_group_db[i].members[m] != NULL; m++) {
            if (strcmp(k_group_db[i].members[m], user) == 0) {
                int dup = 0;
                for (int k = 0; k < count; k++) {
                    if (found[k] == k_group_db[i].gid) {
                        dup = 1;
                        break;
                    }
                }
                if (!dup && count < 8) {
                    found[count++] = k_group_db[i].gid;
                }
            }
        }
    }
    int cap = *ngroups;
    *ngroups = count;
    if (count > cap) {
        if (groups && cap > 0) {
            memcpy(groups, found, (size_t)cap * sizeof(gid_t));
        }
        return -1;
    }
    if (groups && count > 0) {
        memcpy(groups, found, (size_t)count * sizeof(gid_t));
    }
    return count;
}

int initgroups(const char *user, gid_t group) {
    gid_t gids[8];
    int n = 8;
    (void)getgrouplist(user, group, gids, &n);
    return setgroups((size_t)(n > 0 ? n : 0), gids);
}

int getlogin_r(char *name, size_t namesize) {
    if (!name) {
        return EINVAL;
    }
    struct passwd pw;
    struct passwd *res = NULL;
    char pbuf[256];
    int err = getpwuid_r(getuid(), &pw, pbuf, sizeof(pbuf), &res);
    if (err != 0) {
        return err;
    }
    const char *login = res ? res->pw_name : "root";
    size_t need = strlen(login) + 1u;
    if (need > namesize) {
        return ERANGE;
    }
    memcpy(name, login, need);
    return 0;
}

char *getlogin(void) {
    static char g_login_buf[32];
    int err = getlogin_r(g_login_buf, sizeof(g_login_buf));
    if (err != 0) {
        errno = err;
        return NULL;
    }
    return g_login_buf;
}
