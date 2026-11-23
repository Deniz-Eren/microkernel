/**
 * @file     microkernel_compat.c
 * @brief    Minimal Microkernel runtime shims for OpenSSH Portable
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

/* need:: COMP_IPC_0021 */

#include "config.h"
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>
#include "openbsd-compat/getrrsetbyname.h"

int openpty(int *amaster, int *aslave, char *name, const struct termios *termp,
            const struct winsize *winp) {
    if (!amaster || !aslave) {
        errno = EFAULT;
        return -1;
    }
    int master = posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0) {
        return -1;
    }
    char pts[32];
    if (grantpt(master) != 0 || unlockpt(master) != 0 ||
        ptsname_r(master, pts, sizeof(pts)) != 0) {
        int saved = errno;
        (void)close(master);
        errno = saved;
        return -1;
    }
    int slave = open(pts, O_RDWR | O_NOCTTY);
    if (slave < 0) {
        int saved = errno;
        (void)close(master);
        errno = saved;
        return -1;
    }
    if (termp) {
        (void)tcsetattr(slave, TCSAFLUSH, termp);
    }
    if (winp) {
        (void)ioctl(slave, TIOCSWINSZ, winp);
    }
    if (name) {
        (void)strlcpy(name, pts, 32u);
    }
    *amaster = master;
    *aslave = slave;
    return 0;
}

int setitimer(int which, const struct itimerval *value,
              struct itimerval *ovalue) {
    if (which != ITIMER_REAL || !value) {
        errno = EINVAL;
        return -1;
    }
    unsigned int sec = (unsigned int)value->it_value.tv_sec;
    if (sec == 0u && value->it_value.tv_usec > 0) {
        sec = 1u;
    }
    unsigned int prev = alarm(sec);
    if (ovalue) {
        memset(ovalue, 0, sizeof(*ovalue));
        ovalue->it_value.tv_sec = (time_t)prev;
    }
    return 0;
}

int getrrsetbyname(const char *hostname, unsigned int rdclass,
                   unsigned int rdtype, unsigned int flags,
                   struct rrsetinfo **res) {
    (void)hostname;
    (void)rdclass;
    (void)rdtype;
    (void)flags;
    if (res) {
        *res = NULL;
    }
    return ERRSET_FAIL;
}

void freerrset(struct rrsetinfo *rrset) {
    (void)rrset;
}

int _ssh_compat_getentropy(void *s, size_t len) {
    return getentropy(s, len);
}

int fsync(int fd) {
    if (fcntl(fd, F_GETFD) < 0) {
        return -1;
    }
    return 0;
}

int isascii(int c) {
    return (c & ~0x7F) == 0;
}

int wcwidth(wchar_t wc) {
    if (wc == 0) {
        return 0;
    }
    if (wc < 32 || (wc >= 0x7F && wc < 0xA0)) {
        return -1;
    }
    return 1;
}
