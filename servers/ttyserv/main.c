/**
 * @file     main.c
 * @brief    Ring 3 serial TTY (/dev/tty) and PTY (/dev/ptmx, /dev/pts/N) server
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fd.h>
#include <sys/intr.h>
#include <sys/ipc.h>
#include <sys/microkernel.h>
#include <sys/thread.h>
#include <sys/tty.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "uart_backend.h"

/* need:: COMP_IPC_0010 COMP_IPC_0020 COMP_PROC_0009 */

#define DEFAULT_UART_BASE 0x3F8u
#define DEFAULT_UART_IRQ 4u

static uint16_t g_uart_base = (uint16_t)DEFAULT_UART_BASE;
static uint32_t g_uart_irq = DEFAULT_UART_IRQ;

#define EDIT_CAP 512u
#define READY_CAP 2048u
#define PTY_MBUF_CAP 4096u
#define PTY_MAX 8u
#define RWAIT_MAX 24u
#define WWAIT_MAX 16u
#define PWAIT_MAX 16u

typedef struct {
    uint8_t used;
    uint8_t is_pty;
    uint8_t unlocked;
    uint8_t slave_opened;
    uint32_t master_refs;
    uint32_t slave_refs;
    struct termios tio;
    struct winsize ws;
    pid_t fg_pgrp;
    pid_t sid;
    uint8_t edit[EDIT_CAP];
    uint32_t edit_len;
    uint8_t ready[READY_CAP];
    uint32_t ready_head;
    uint32_t ready_len;
    uint32_t eof_pending;
    uint8_t mbuf[PTY_MBUF_CAP];
    uint32_t mbuf_head;
    uint32_t mbuf_len;
} tty_dev_t;

typedef struct {
    uint8_t used;
    uint32_t msgid;
    uint32_t handle;
    uint32_t want_len;
} tty_rwait_t;

typedef struct {
    uint8_t used;
    uint32_t msgid;
    uint32_t handle;
    uint32_t len;
    uint8_t data[TTY_DATA_MAX];
} tty_wwait_t;

typedef struct {
    uint8_t used;
    uint8_t has_deadline;
    uint8_t is_multi;
    uint32_t msgid;
    uint32_t nfds;
    uint64_t deadline_ns;
    tty_poll_item_t items[TTY_POLL_MAX];
} tty_pwait_t;

static tty_dev_t g_con;
static tty_dev_t g_ptys[PTY_MAX];
static tty_rwait_t g_rwait[RWAIT_MAX];
static tty_wwait_t g_wwait[WWAIT_MAX];
static tty_pwait_t g_pwait[PWAIT_MAX];

static volatile unsigned int g_timed_polls;
static int g_self_conn = -1;

static uint64_t now_mono_ns(void) {
    struct timespec ts = {0};
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void ticker_thread(void *arg) {
    (void)arg;
    struct timespec step = {.tv_sec = 0, .tv_nsec = 5000000L}; /* 5 ms */
    for (;;) {
        while (__atomic_load_n(&g_timed_polls, __ATOMIC_ACQUIRE) == 0u) {
            (void)__sys5(SYS_FUTEX, (long)(uintptr_t)&g_timed_polls, FUTEX_WAIT,
                         0, 0, 0);
        }
        clock_nanosleep(CLOCK_MONOTONIC, 0, &step, NULL);
        if (g_self_conn >= 0) {
            (void)SendPulse(g_self_conn, (int)PULSE_CODE_TTY_TICK, 0);
        }
    }
}

static void inc_timed_polls(void) {
    unsigned int prev =
        __atomic_fetch_add(&g_timed_polls, 1u, __ATOMIC_ACQ_REL);
    if (prev == 0u) {
        (void)__sys5(SYS_FUTEX, (long)(uintptr_t)&g_timed_polls, FUTEX_WAKE, 1,
                     0, 0);
    }
}

static void dec_timed_polls(void) {
    unsigned int cur = __atomic_load_n(&g_timed_polls, __ATOMIC_ACQUIRE);
    if (cur > 0u) {
        __atomic_fetch_sub(&g_timed_polls, 1u, __ATOMIC_ACQ_REL);
    }
}

static void init_default_termios(struct termios *tio) {
    memset(tio, 0, sizeof(*tio));
    tio->c_iflag = ICRNL | IXON;
    tio->c_oflag = OPOST | ONLCR;
    tio->c_cflag = CS8 | CREAD | CLOCAL;
    tio->c_lflag = ICANON | ECHO | ECHOE | ECHOK | ISIG;
    tio->c_cc[VEOF] = 0x04u;
    tio->c_cc[VEOL] = 0x00u;
    tio->c_cc[VERASE] = 0x7Fu;
    tio->c_cc[VINTR] = 0x03u;
    tio->c_cc[VKILL] = 0x15u;
    tio->c_cc[VMIN] = 1u;
    tio->c_cc[VQUIT] = 0x1Cu;
    tio->c_cc[VSTART] = 0x11u;
    tio->c_cc[VSTOP] = 0x13u;
    tio->c_cc[VSUSP] = 0x1Au;
    tio->c_cc[VTIME] = 0u;
    tio->__c_ispeed = B115200;
    tio->__c_ospeed = B115200;
}

static void init_dev(tty_dev_t *d, uint8_t is_pty) {
    memset(d, 0, sizeof(*d));
    d->used = 1u;
    d->is_pty = is_pty;
    init_default_termios(&d->tio);
    d->ws.ws_row = 24u;
    d->ws.ws_col = 80u;
}

static tty_dev_t *resolve_handle(uint32_t handle, int *is_master_out) {
    if (handle == 0u) {
        if (is_master_out) {
            *is_master_out = 0;
        }
        return &g_con;
    }
    uint32_t slot = (handle >> 1) - 1u;
    if ((handle >> 1) == 0u || slot >= PTY_MAX || !g_ptys[slot].used) {
        return NULL;
    }
    if (is_master_out) {
        *is_master_out = ((handle & 1u) == 0u) ? 1 : 0;
    }
    return &g_ptys[slot];
}

static void ready_push(tty_dev_t *d, uint8_t c) {
    if (d->ready_len >= READY_CAP) {
        return;
    }
    uint32_t tail = (d->ready_head + d->ready_len) % READY_CAP;
    d->ready[tail] = c;
    d->ready_len++;
}

static uint32_t ready_pop(tty_dev_t *d, uint8_t *dst, uint32_t max_n) {
    uint32_t take = d->ready_len < max_n ? d->ready_len : max_n;
    for (uint32_t i = 0; i < take; i++) {
        dst[i] = d->ready[(d->ready_head + i) % READY_CAP];
    }
    d->ready_head = (d->ready_head + take) % READY_CAP;
    d->ready_len -= take;
    return take;
}

static void mbuf_push(tty_dev_t *d, uint8_t c) {
    if (d->mbuf_len >= PTY_MBUF_CAP) {
        return;
    }
    uint32_t tail = (d->mbuf_head + d->mbuf_len) % PTY_MBUF_CAP;
    d->mbuf[tail] = c;
    d->mbuf_len++;
}

static uint32_t mbuf_pop(tty_dev_t *d, uint8_t *dst, uint32_t max_n) {
    uint32_t take = d->mbuf_len < max_n ? d->mbuf_len : max_n;
    for (uint32_t i = 0; i < take; i++) {
        dst[i] = d->mbuf[(d->mbuf_head + i) % PTY_MBUF_CAP];
    }
    d->mbuf_head = (d->mbuf_head + take) % PTY_MBUF_CAP;
    d->mbuf_len -= take;
    return take;
}

static uint32_t min_read_ready(const tty_dev_t *d) {
    if (d->tio.c_lflag & ICANON) {
        return 1u;
    }
    uint32_t vmin = (uint32_t)d->tio.c_cc[VMIN];
    return vmin == 0u ? 1u : vmin;
}

static int slave_is_readable(const tty_dev_t *d) {
    if (d->eof_pending > 0u) {
        return 1;
    }
    if (d->is_pty && d->master_refs == 0u) {
        return 1;
    }
    return d->ready_len >= min_read_ready(d);
}

static int master_is_readable(const tty_dev_t *d) {
    if (d->mbuf_len > 0u) {
        return 1;
    }
    return (d->slave_opened && d->slave_refs == 0u) ? 1 : 0;
}

static int16_t eval_handle_events(uint32_t handle, int16_t events) {
    int is_master = 0;
    tty_dev_t *d = resolve_handle(handle, &is_master);
    if (!d) {
        return POLLNVAL;
    }
    int16_t rev = 0;
    if (is_master) {
        if (d->mbuf_len > 0u) {
            rev = (int16_t)(rev | (events & (POLLIN | POLLRDNORM)));
        }
        if (d->slave_opened && d->slave_refs == 0u) {
            rev = (int16_t)(rev | POLLHUP);
            rev = (int16_t)(rev | (events & (POLLIN | POLLRDNORM)));
        }
        if (d->ready_len < READY_CAP) {
            rev = (int16_t)(rev | (events & (POLLOUT | POLLWRNORM)));
        }
    } else {
        if (slave_is_readable(d)) {
            rev = (int16_t)(rev | (events & (POLLIN | POLLRDNORM)));
        }
        if (d->is_pty && d->master_refs == 0u) {
            rev = (int16_t)(rev | POLLHUP);
        } else if (!d->is_pty || d->mbuf_len < PTY_MBUF_CAP) {
            rev = (int16_t)(rev | (events & (POLLOUT | POLLWRNORM)));
        }
    }
    return rev;
}

static uint32_t write_slave_to_pty(tty_dev_t *d, const uint8_t *src,
                                   uint32_t len) {
    int do_onlcr =
        ((d->tio.c_oflag & OPOST) != 0u && (d->tio.c_oflag & ONLCR) != 0u);
    uint32_t done = 0u;
    while (done < len) {
        uint8_t c = src[done];
        uint32_t need = (do_onlcr && c == '\n') ? 2u : 1u;
        if (PTY_MBUF_CAP - d->mbuf_len < need) {
            break;
        }
        if (do_onlcr && c == '\n') {
            mbuf_push(d, '\r');
        }
        mbuf_push(d, c);
        done++;
    }
    return done;
}

static void wake_waiters(void) {
    /* 1. Wake blocked readers when their handle has data or EOF. */
    for (uint32_t i = 0; i < RWAIT_MAX; i++) {
        if (!g_rwait[i].used) {
            continue;
        }
        int is_master = 0;
        tty_dev_t *d = resolve_handle(g_rwait[i].handle, &is_master);
        tty_rep_t rep = {0};
        if (!d) {
            rep.err = -EBADF;
            uint32_t msgid = g_rwait[i].msgid;
            g_rwait[i].used = 0u;
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            continue;
        }
        uint32_t want = g_rwait[i].want_len > TTY_DATA_MAX
                            ? TTY_DATA_MAX
                            : g_rwait[i].want_len;
        if (is_master) {
            if (!master_is_readable(d)) {
                continue;
            }
            if (d->mbuf_len > 0u) {
                rep.len = mbuf_pop(d, rep.u.data, want);
            } else {
                rep.len = 0u;
            }
        } else {
            if (!slave_is_readable(d)) {
                continue;
            }
            if (d->ready_len > 0u) {
                rep.len = ready_pop(d, rep.u.data, want);
            } else if (d->eof_pending > 0u) {
                d->eof_pending--;
                rep.len = 0u;
            } else {
                rep.len = 0u;
            }
        }
        rep.err = 0;
        uint32_t msgid = g_rwait[i].msgid;
        g_rwait[i].used = 0u;
        (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
    }

    /* 2. Wake blocked PTY slave writers when master buffer drains. */
    for (uint32_t i = 0; i < WWAIT_MAX; i++) {
        if (!g_wwait[i].used) {
            continue;
        }
        int is_master = 0;
        tty_dev_t *d = resolve_handle(g_wwait[i].handle, &is_master);
        tty_rep_t rep = {0};
        if (!d || is_master || d->master_refs == 0u) {
            rep.err = -EIO;
            uint32_t msgid = g_wwait[i].msgid;
            g_wwait[i].used = 0u;
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            continue;
        }
        uint32_t wrote = write_slave_to_pty(d, g_wwait[i].data, g_wwait[i].len);
        if (wrote > 0u) {
            rep.err = 0;
            rep.len = wrote;
            uint32_t msgid = g_wwait[i].msgid;
            g_wwait[i].used = 0u;
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
        }
    }

    /* 3. Wake blocked pollers when ready or timed out. */
    uint64_t now = 0;
    for (uint32_t i = 0; i < PWAIT_MAX; i++) {
        if (!g_pwait[i].used) {
            continue;
        }
        uint32_t ready = 0u;
        for (uint32_t k = 0; k < g_pwait[i].nfds; k++) {
            int16_t rev = eval_handle_events(g_pwait[i].items[k].handle,
                                             g_pwait[i].items[k].events);
            g_pwait[i].items[k].revents = rev;
            if (rev != 0) {
                ready++;
            }
        }
        int expired = 0;
        if (ready == 0u && g_pwait[i].has_deadline) {
            if (now == 0) {
                now = now_mono_ns();
            }
            if (now >= g_pwait[i].deadline_ns) {
                expired = 1;
            }
        }
        if (ready > 0u || expired) {
            uint32_t msgid = g_pwait[i].msgid;
            if (g_pwait[i].has_deadline) {
                dec_timed_polls();
            }
            if (g_pwait[i].is_multi) {
                tty_poll_rep_t prep = {0};
                prep.err = 0;
                prep.ready = ready;
                memcpy(prep.items, g_pwait[i].items,
                       g_pwait[i].nfds * sizeof(tty_poll_item_t));
                g_pwait[i].used = 0u;
                (void)ReplyMsg((int)msgid, &prep, sizeof(prep));
            } else {
                tty_rep_t rep = {0};
                rep.err = 0;
                rep.revents = g_pwait[i].items[0].revents;
                g_pwait[i].used = 0u;
                (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            }
        }
    }
}

static void commit_edit_buffer(tty_dev_t *d) {
    for (uint32_t i = 0; i < d->edit_len; i++) {
        ready_push(d, d->edit[i]);
    }
    d->edit_len = 0u;
}

static void dev_putc_raw(tty_dev_t *d, uint8_t c) {
    if (d->is_pty) {
        mbuf_push(d, c);
    } else {
        tty_uart_putc(c);
    }
}

static void tty_tx_byte(tty_dev_t *d, uint8_t c) {
    if ((d->tio.c_oflag & OPOST) != 0u && (d->tio.c_oflag & ONLCR) != 0u &&
        c == '\n') {
        dev_putc_raw(d, '\r');
    }
    dev_putc_raw(d, c);
}

static void tty_feed_byte(tty_dev_t *d, uint8_t c, int allow_echo) {
    if (d->tio.c_iflag & ISTRIP) {
        c &= 0x7Fu;
    }
    if (c == '\r') {
        if (d->tio.c_iflag & IGNCR) {
            return;
        }
        if (d->tio.c_iflag & ICRNL) {
            c = '\n';
        }
    } else if (c == '\n' && (d->tio.c_iflag & INLCR)) {
        c = '\r';
    }

    int do_echo = allow_echo && ((d->tio.c_lflag & ECHO) != 0u);

    if ((d->tio.c_lflag & ISIG) != 0u) {
        int sig = 0;
        char echo_ch = 0;
        if (d->tio.c_cc[VINTR] != 0u && c == d->tio.c_cc[VINTR]) {
            sig = SIGINT;
            echo_ch = 'C';
        } else if (d->tio.c_cc[VQUIT] != 0u && c == d->tio.c_cc[VQUIT]) {
            sig = SIGQUIT;
            echo_ch = '\\';
        }
        if (sig != 0) {
            if (do_echo) {
                tty_tx_byte(d, '^');
                tty_tx_byte(d, (uint8_t)echo_ch);
                tty_tx_byte(d, '\n');
            }
            if ((d->tio.c_lflag & NOFLSH) == 0u) {
                d->edit_len = 0u;
                d->ready_head = 0u;
                d->ready_len = 0u;
                d->eof_pending = 0u;
            }
            if (d->fg_pgrp > 0) {
                (void)kill(-d->fg_pgrp, sig);
            }
            wake_waiters();
            return;
        }
    }

    if (d->tio.c_lflag & ICANON) {
        if (c == d->tio.c_cc[VERASE] || c == '\b') {
            if (d->edit_len > 0u) {
                d->edit_len--;
                if (do_echo && (d->tio.c_lflag & ECHOE) != 0u) {
                    dev_putc_raw(d, '\b');
                    dev_putc_raw(d, ' ');
                    dev_putc_raw(d, '\b');
                }
            }
            wake_waiters();
            return;
        }
        if (c == d->tio.c_cc[VKILL]) {
            if (do_echo && (d->tio.c_lflag & ECHOE) != 0u) {
                while (d->edit_len > 0u) {
                    d->edit_len--;
                    dev_putc_raw(d, '\b');
                    dev_putc_raw(d, ' ');
                    dev_putc_raw(d, '\b');
                }
            } else {
                d->edit_len = 0u;
            }
            wake_waiters();
            return;
        }
        if (c == d->tio.c_cc[VEOF]) {
            if (d->edit_len > 0u) {
                commit_edit_buffer(d);
            } else {
                d->eof_pending++;
            }
            wake_waiters();
            return;
        }
        if (d->edit_len < EDIT_CAP) {
            d->edit[d->edit_len++] = c;
            if (do_echo) {
                tty_tx_byte(d, c);
            }
        }
        int is_eol = (c == '\n') ||
                     (d->tio.c_cc[VEOL] != 0u && c == d->tio.c_cc[VEOL]) ||
                     (d->edit_len >= EDIT_CAP);
        if (is_eol) {
            commit_edit_buffer(d);
            wake_waiters();
        } else if (do_echo && d->is_pty) {
            wake_waiters();
        }
    } else {
        ready_push(d, c);
        if (do_echo) {
            tty_tx_byte(d, c);
        }
        wake_waiters();
    }
}

static void drain_com1_rx(int feed_ldisc) {
    uint8_t b = 0;
    while (tty_uart_getc(&b)) {
        if (feed_ldisc) {
            tty_feed_byte(&g_con, b, 1);
        }
    }
}

static int register_tty_service(int chanid) {
    for (int tries = 0; tries < 200; tries++) {
        long ns = OpenConnection(1, 1);
        if (ns >= 0) {
            name_msg_t req = {0};
            req.type = NAME_MSG_ATTACH;
            req.chanid = (uint32_t)chanid;
            (void)strlcpy(req.path, "/dev/tty", sizeof(req.path));
            name_reply_t rep = {0};
            long r = SendMsg((int)ns, &req, sizeof(req), &rep, sizeof(rep));
            CloseConnection((int)ns);
            if (r >= 0 && rep.err == 0) {
                return 0;
            }
        }
        struct timespec d = {.tv_sec = 0, .tv_nsec = 5000000L};
        clock_nanosleep(CLOCK_MONOTONIC, 0, &d, NULL);
    }
    return -1;
}

static void tty_close_handle(uint32_t handle) {
    int is_master = 0;
    tty_dev_t *d = resolve_handle(handle, &is_master);
    if (!d || !d->is_pty) {
        return;
    }
    if (is_master) {
        if (d->master_refs > 0u) {
            d->master_refs--;
        }
        if (d->master_refs == 0u && d->slave_refs > 0u && d->fg_pgrp > 0) {
            (void)kill(-d->fg_pgrp, SIGHUP);
        }
    } else {
        if (d->slave_refs > 0u) {
            d->slave_refs--;
        }
    }
    wake_waiters();
    if (d->master_refs == 0u && d->slave_refs == 0u) {
        d->used = 0u;
    }
}

static void handle_read(uint32_t msgid, const tty_req_t *req) {
    tty_rep_t rep = {0};
    int is_master = 0;
    tty_dev_t *d = resolve_handle(req->handle, &is_master);
    if (!d) {
        rep.err = -EBADF;
        (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    uint32_t want = req->len > TTY_DATA_MAX ? TTY_DATA_MAX : req->len;
    if (want == 0u) {
        (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    if (is_master) {
        if (master_is_readable(d)) {
            if (d->mbuf_len > 0u) {
                rep.len = mbuf_pop(d, rep.u.data, want);
            } else {
                rep.len = 0u;
            }
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            wake_waiters();
            return;
        }
        if ((req->flags & (uint32_t)O_NONBLOCK) != 0u) {
            rep.err = -EAGAIN;
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
    } else {
        if (slave_is_readable(d)) {
            if (d->ready_len > 0u) {
                rep.len = ready_pop(d, rep.u.data, want);
            } else if (d->eof_pending > 0u) {
                d->eof_pending--;
                rep.len = 0u;
            } else {
                rep.len = 0u;
            }
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            wake_waiters();
            return;
        }
        if ((req->flags & (uint32_t)O_NONBLOCK) != 0u ||
            (!(d->tio.c_lflag & ICANON) && d->tio.c_cc[VMIN] == 0u)) {
            rep.err = (req->flags & (uint32_t)O_NONBLOCK) ? -EAGAIN : 0;
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
    }
    for (uint32_t i = 0; i < RWAIT_MAX; i++) {
        if (!g_rwait[i].used) {
            g_rwait[i].used = 1u;
            g_rwait[i].msgid = msgid;
            g_rwait[i].handle = req->handle;
            g_rwait[i].want_len = want;
            return;
        }
    }
    rep.err = -EAGAIN;
    (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
}

static void handle_write(uint32_t msgid, const tty_req_t *req) {
    tty_rep_t rep = {0};
    int is_master = 0;
    tty_dev_t *d = resolve_handle(req->handle, &is_master);
    if (!d) {
        rep.err = -EBADF;
        (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    uint32_t len = req->len > TTY_DATA_MAX ? TTY_DATA_MAX : req->len;
    if (len == 0u) {
        (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    if (!d->is_pty) {
        uint8_t out[TTY_DATA_MAX * 2u];
        size_t out_len = 0u;
        int do_onlcr =
            ((d->tio.c_oflag & OPOST) != 0u && (d->tio.c_oflag & ONLCR) != 0u);
        for (uint32_t i = 0; i < len; i++) {
            uint8_t c = req->u.data[i];
            if (do_onlcr && c == '\n') {
                out[out_len++] = '\r';
            }
            out[out_len++] = c;
        }
        tty_uart_write(out, out_len);
        rep.err = 0;
        rep.len = len;
        (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    if (is_master) {
        for (uint32_t i = 0; i < len; i++) {
            tty_feed_byte(d, req->u.data[i], 1);
        }
        rep.err = 0;
        rep.len = len;
        (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
        wake_waiters();
        return;
    }
    /* PTY slave write -> master output buffer. */
    if (d->master_refs == 0u) {
        rep.err = -EIO;
        (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    uint32_t wrote = write_slave_to_pty(d, req->u.data, len);
    if (wrote > 0u) {
        rep.err = 0;
        rep.len = wrote;
        (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
        wake_waiters();
        return;
    }
    if ((req->flags & (uint32_t)O_NONBLOCK) != 0u) {
        rep.err = -EAGAIN;
        (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    for (uint32_t i = 0; i < WWAIT_MAX; i++) {
        if (!g_wwait[i].used) {
            g_wwait[i].used = 1u;
            g_wwait[i].msgid = msgid;
            g_wwait[i].handle = req->handle;
            g_wwait[i].len = len;
            memcpy(g_wwait[i].data, req->u.data, len);
            return;
        }
    }
    rep.err = -EAGAIN;
    (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
}

static void handle_poll_single(uint32_t msgid, const tty_req_t *req) {
    tty_rep_t rep = {0};
    int16_t events = (int16_t)req->arg;
    int16_t rev = eval_handle_events(req->handle, events);
    if (rev != 0 || req->timeout_ns == 0) {
        rep.err = 0;
        rep.revents = rev;
        (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    for (uint32_t i = 0; i < PWAIT_MAX; i++) {
        if (!g_pwait[i].used) {
            g_pwait[i].used = 1u;
            g_pwait[i].is_multi = 0u;
            g_pwait[i].msgid = msgid;
            g_pwait[i].nfds = 1u;
            g_pwait[i].items[0].handle = req->handle;
            g_pwait[i].items[0].events = events;
            g_pwait[i].items[0].revents = 0;
            if (req->timeout_ns > 0) {
                g_pwait[i].has_deadline = 1u;
                g_pwait[i].deadline_ns =
                    now_mono_ns() + (uint64_t)req->timeout_ns;
                inc_timed_polls();
            } else {
                g_pwait[i].has_deadline = 0u;
                g_pwait[i].deadline_ns = 0ULL;
            }
            return;
        }
    }
    rep.err = -EAGAIN;
    (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
}

static void handle_poll_multi(uint32_t msgid, const tty_poll_req_t *req) {
    tty_poll_rep_t rep = {0};
    if (req->nfds > TTY_POLL_MAX) {
        rep.err = -EINVAL;
        (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    tty_poll_item_t items[TTY_POLL_MAX];
    uint32_t ready = 0u;
    for (uint32_t k = 0; k < req->nfds; k++) {
        items[k] = req->items[k];
        items[k].revents = eval_handle_events(items[k].handle, items[k].events);
        if (items[k].revents != 0) {
            ready++;
        }
    }
    if (ready > 0u || req->timeout_ns == 0) {
        rep.err = 0;
        rep.ready = ready;
        memcpy(rep.items, items, req->nfds * sizeof(tty_poll_item_t));
        (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    for (uint32_t i = 0; i < PWAIT_MAX; i++) {
        if (!g_pwait[i].used) {
            g_pwait[i].used = 1u;
            g_pwait[i].is_multi = 1u;
            g_pwait[i].msgid = msgid;
            g_pwait[i].nfds = req->nfds;
            memcpy(g_pwait[i].items, items,
                   req->nfds * sizeof(tty_poll_item_t));
            if (req->timeout_ns > 0) {
                g_pwait[i].has_deadline = 1u;
                g_pwait[i].deadline_ns =
                    now_mono_ns() + (uint64_t)req->timeout_ns;
                inc_timed_polls();
            } else {
                g_pwait[i].has_deadline = 0u;
                g_pwait[i].deadline_ns = 0ULL;
            }
            return;
        }
    }
    rep.err = -EAGAIN;
    (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
}

int main(int argc, char **argv) {
    for (int i = 1; i + 1 < argc; i++) {
        if (strcmp(argv[i], "-p") == 0) {
            unsigned long p = strtoul(argv[++i], NULL, 0);
            if (p > 0UL && p <= 0xFFF8UL) {
                g_uart_base = (uint16_t)p;
            }
        } else if (strcmp(argv[i], "-i") == 0) {
            unsigned long irq = strtoul(argv[++i], NULL, 0);
            if (irq > 0UL && irq < 24UL) {
                g_uart_irq = (uint32_t)irq;
            }
        }
    }

    init_dev(&g_con, 0u);

    int chan = (int)CreateChannel();
    if (chan < 0) {
        printf("[FAIL] ttyserv CreateChannel failed\n");
        return 1;
    }
    if (tty_uart_init(g_uart_base, g_uart_irq, chan) != 0) {
        return 1;
    }

    if (register_tty_service(chan) != 0) {
        printf("[FAIL] ttyserv failed to register /dev/tty\n");
        return 1;
    }
    printf("[OK] ttyserv registered /dev/tty (chanid=%d)\n", chan);

    long my_pid = __sys0(SYS_PROC_GETPID);
    if (my_pid > 0) {
        g_self_conn = (int)OpenConnection((uint32_t)my_pid, (uint32_t)chan);
    }
    (void)ThreadSpawn(ticker_thread, NULL, 95);

    union {
        uint32_t op;
        tty_req_t req;
        tty_poll_req_t preq;
        ipc_pulse_t pulse;
        uint8_t raw[512];
    } msg;
    tty_rep_t rep;
    ipc_recv_info_t info;
    for (;;) {
        memset(&msg, 0, sizeof(msg));
        memset(&rep, 0, sizeof(rep));
        memset(&info, 0, sizeof(info));
        long msgid = ReceiveMsg(chan, &msg, sizeof(msg), &info);
        if (msgid < 0) {
            continue;
        }
        if (msgid == 0) {
            if (msg.pulse.code == PULSE_CODE_IRQ &&
                msg.pulse.value == g_uart_irq) {
                drain_com1_rx(1);
                (void)IntrUnmask(g_uart_irq);
            } else if (msg.pulse.code == PULSE_CODE_TTY_TICK) {
                wake_waiters();
            } else if (msg.pulse.code == PULSE_CODE_FD_CLOSE) {
                tty_close_handle((uint32_t)msg.pulse.value);
            }
            continue;
        }

        if (msg.op == TTY_OP_POLL_MULTI) {
            handle_poll_multi((uint32_t)msgid, &msg.preq);
            continue;
        }

        const tty_req_t *req = &msg.req;
        switch (req->op) {
        case TTY_OP_READ:
            handle_read((uint32_t)msgid, req);
            break;
        case TTY_OP_WRITE:
            handle_write((uint32_t)msgid, req);
            break;
        case TTY_OP_PTY_OPEN_MASTER: {
            uint32_t found = PTY_MAX;
            for (uint32_t i = 0; i < PTY_MAX; i++) {
                if (!g_ptys[i].used) {
                    found = i;
                    break;
                }
            }
            if (found >= PTY_MAX) {
                rep.err = -ENFILE;
            } else {
                init_dev(&g_ptys[found], 1u);
                g_ptys[found].master_refs = 1u;
                rep.err = 0;
                rep.handle = (found + 1u) << 1;
                rep.len = found;
            }
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        }
        case TTY_OP_PTY_UNLOCK: {
            int is_master = 0;
            tty_dev_t *d = resolve_handle(req->handle, &is_master);
            if (!d || !d->is_pty || !is_master) {
                rep.err = -EINVAL;
            } else {
                d->unlocked = 1u;
                rep.err = 0;
            }
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        }
        case TTY_OP_PTY_OPEN_SLAVE: {
            uint32_t slot = req->arg;
            if (slot >= PTY_MAX || !g_ptys[slot].used ||
                g_ptys[slot].master_refs == 0u) {
                rep.err = -ENOENT;
            } else if (!g_ptys[slot].unlocked) {
                rep.err = -EIO;
            } else {
                g_ptys[slot].slave_refs++;
                g_ptys[slot].slave_opened = 1u;
                rep.err = 0;
                rep.handle = ((slot + 1u) << 1) | 1u;
            }
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        }
        case TTY_OP_DUP: {
            int is_master = 0;
            tty_dev_t *d = resolve_handle(req->handle, &is_master);
            if (!d) {
                rep.err = -EBADF;
            } else {
                if (d->is_pty) {
                    if (is_master) {
                        d->master_refs++;
                    } else {
                        d->slave_refs++;
                    }
                }
                rep.err = 0;
            }
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        }
        case TTY_OP_CLOSE:
            tty_close_handle(req->handle);
            rep.err = 0;
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        case TTY_OP_GETATTR: {
            tty_dev_t *d = resolve_handle(req->handle, NULL);
            if (!d) {
                rep.err = -EBADF;
            } else {
                rep.err = 0;
                rep.u.tio = d->tio;
            }
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        }
        case TTY_OP_SETATTR: {
            tty_dev_t *d = resolve_handle(req->handle, NULL);
            if (!d) {
                rep.err = -EBADF;
            } else if (req->arg != (uint32_t)TCSANOW &&
                       req->arg != (uint32_t)TCSADRAIN &&
                       req->arg != (uint32_t)TCSAFLUSH) {
                rep.err = -EINVAL;
            } else {
                if (req->arg == (uint32_t)TCSAFLUSH) {
                    d->edit_len = 0u;
                    d->ready_head = 0u;
                    d->ready_len = 0u;
                    d->eof_pending = 0u;
                }
                d->tio = req->u.tio;
                rep.err = 0;
                wake_waiters();
            }
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        }
        case TTY_OP_FLUSH: {
            tty_dev_t *d = resolve_handle(req->handle, NULL);
            if (!d) {
                rep.err = -EBADF;
            } else if (req->arg != (uint32_t)TCIFLUSH &&
                       req->arg != (uint32_t)TCOFLUSH &&
                       req->arg != (uint32_t)TCIOFLUSH) {
                rep.err = -EINVAL;
            } else {
                if (req->arg == (uint32_t)TCIFLUSH ||
                    req->arg == (uint32_t)TCIOFLUSH) {
                    d->edit_len = 0u;
                    d->ready_head = 0u;
                    d->ready_len = 0u;
                    d->eof_pending = 0u;
                }
                if (d->is_pty && (req->arg == (uint32_t)TCOFLUSH ||
                                  req->arg == (uint32_t)TCIOFLUSH)) {
                    d->mbuf_head = 0u;
                    d->mbuf_len = 0u;
                }
                rep.err = 0;
                wake_waiters();
            }
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        }
        case TTY_OP_POLL:
            handle_poll_single((uint32_t)msgid, req);
            break;
        case TTY_OP_INJECT: {
            tty_dev_t *d = resolve_handle(req->handle, NULL);
            if (!d) {
                rep.err = -EBADF;
            } else {
                uint32_t len =
                    req->len > TTY_DATA_MAX ? TTY_DATA_MAX : req->len;
                for (uint32_t i = 0; i < len; i++) {
                    tty_feed_byte(d, req->u.data[i], 0);
                }
                rep.err = 0;
                rep.len = len;
            }
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        }
        case TTY_OP_DRAIN:
        case TTY_OP_SENDBREAK: {
            tty_dev_t *d = resolve_handle(req->handle, NULL);
            rep.err = d ? 0 : -EBADF;
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        }
        case TTY_OP_FLOW: {
            tty_dev_t *d = resolve_handle(req->handle, NULL);
            if (!d) {
                rep.err = -EBADF;
            } else if (req->arg != (uint32_t)TCOOFF &&
                       req->arg != (uint32_t)TCOON &&
                       req->arg != (uint32_t)TCIOFF &&
                       req->arg != (uint32_t)TCION) {
                rep.err = -EINVAL;
            } else {
                rep.err = 0;
            }
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        }
        case TTY_OP_GETPGRP: {
            tty_dev_t *d = resolve_handle(req->handle, NULL);
            if (!d) {
                rep.err = -EBADF;
            } else {
                rep.err = 0;
                rep.len = (uint32_t)(d->fg_pgrp > 0 ? d->fg_pgrp : 1);
            }
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        }
        case TTY_OP_SETPGRP: {
            tty_dev_t *d = resolve_handle(req->handle, NULL);
            if (!d) {
                rep.err = -EBADF;
            } else if ((int32_t)req->arg <= 0) {
                rep.err = -EINVAL;
            } else {
                d->fg_pgrp = (pid_t)req->arg;
                pid_t sid = getsid(d->fg_pgrp);
                if (sid > 0) {
                    d->sid = sid;
                } else if (d->sid <= 0) {
                    d->sid = d->fg_pgrp;
                }
                rep.err = 0;
            }
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        }
        case TTY_OP_SCTTY: {
            tty_dev_t *d = resolve_handle(req->handle, NULL);
            if (!d) {
                rep.err = -EBADF;
            } else {
                if ((int32_t)req->arg > 0) {
                    d->sid = (pid_t)req->arg;
                }
                if ((int32_t)req->flags > 0) {
                    d->fg_pgrp = (pid_t)req->flags;
                }
                rep.err = 0;
            }
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        }
        case TTY_OP_GETSID: {
            tty_dev_t *d = resolve_handle(req->handle, NULL);
            if (!d) {
                rep.err = -EBADF;
            } else {
                rep.err = 0;
                rep.len =
                    (uint32_t)(d->sid > 0 ? d->sid
                                          : (d->fg_pgrp > 0 ? d->fg_pgrp : 0));
            }
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        }
        case TTY_OP_GETWINSIZE: {
            tty_dev_t *d = resolve_handle(req->handle, NULL);
            if (!d) {
                rep.err = -EBADF;
            } else {
                rep.err = 0;
                rep.u.ws = d->ws;
            }
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        }
        case TTY_OP_SETWINSIZE: {
            tty_dev_t *d = resolve_handle(req->handle, NULL);
            int send_winch = 0;
            pid_t target_pg = 0;
            if (!d) {
                rep.err = -EBADF;
            } else {
                int changed = (d->ws.ws_row != req->u.ws.ws_row ||
                               d->ws.ws_col != req->u.ws.ws_col);
                d->ws = req->u.ws;
                rep.err = 0;
                if (changed && d->fg_pgrp > 0) {
                    send_winch = 1;
                    target_pg = d->fg_pgrp;
                }
            }
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            if (send_winch) {
                (void)kill(-target_pg, SIGWINCH);
            }
            break;
        }
        case TTY_OP_FIONREAD: {
            int is_master = 0;
            tty_dev_t *d = resolve_handle(req->handle, &is_master);
            if (!d) {
                rep.err = -EBADF;
            } else {
                rep.err = 0;
                rep.len = is_master ? d->mbuf_len : d->ready_len;
            }
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        }
        default:
            rep.err = -ENOSYS;
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        }
    }
}
