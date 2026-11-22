/**
 * @file     main.c
 * @brief    Ring 3 serial TTY server (/dev/tty) with COM1 IRQ 4 and termios
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
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
#include "uart_backend.h"

/* need:: COMP_IPC_0010 */

#define DEFAULT_UART_BASE 0x3F8u
#define DEFAULT_UART_IRQ 4u

static uint16_t g_uart_base = (uint16_t)DEFAULT_UART_BASE;
static uint32_t g_uart_irq = DEFAULT_UART_IRQ;

#define EDIT_CAP 512u
#define READY_CAP 2048u
#define RWAIT_MAX 16u
#define PWAIT_MAX 16u

typedef struct {
    uint8_t used;
    uint32_t msgid;
    uint32_t want_len;
} tty_rwait_t;

typedef struct {
    uint8_t used;
    uint8_t has_deadline;
    uint32_t msgid;
    int16_t events;
    uint64_t deadline_ns;
} tty_pwait_t;

static struct termios g_tio;
static uint8_t g_edit[EDIT_CAP];
static uint32_t g_edit_len;

static uint8_t g_ready[READY_CAP];
static uint32_t g_ready_head;
static uint32_t g_ready_len;
static uint32_t g_eof_pending;

static tty_rwait_t g_rwait[RWAIT_MAX];
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

static void init_default_termios(void) {
    memset(&g_tio, 0, sizeof(g_tio));
    g_tio.c_iflag = ICRNL | IXON;
    g_tio.c_oflag = OPOST | ONLCR;
    g_tio.c_cflag = CS8 | CREAD | CLOCAL;
    g_tio.c_lflag = ICANON | ECHO | ECHOE | ECHOK | ISIG;
    g_tio.c_cc[VEOF] = 0x04u;   /* Ctrl-D */
    g_tio.c_cc[VEOL] = 0x00u;   /* disabled */
    g_tio.c_cc[VERASE] = 0x7Fu; /* DEL */
    g_tio.c_cc[VINTR] = 0x03u;  /* Ctrl-C */
    g_tio.c_cc[VKILL] = 0x15u;  /* Ctrl-U */
    g_tio.c_cc[VMIN] = 1u;
    g_tio.c_cc[VQUIT] = 0x1Cu;
    g_tio.c_cc[VSTART] = 0x11u;
    g_tio.c_cc[VSTOP] = 0x13u;
    g_tio.c_cc[VSUSP] = 0x1Au;
    g_tio.c_cc[VTIME] = 0u;
    g_tio.__c_ispeed = B115200;
    g_tio.__c_ospeed = B115200;
}

static void ready_push(uint8_t c) {
    if (g_ready_len >= READY_CAP) {
        return;
    }
    uint32_t tail = (g_ready_head + g_ready_len) % READY_CAP;
    g_ready[tail] = c;
    g_ready_len++;
}

static uint32_t ready_pop(uint8_t *dst, uint32_t max_n) {
    uint32_t take = g_ready_len < max_n ? g_ready_len : max_n;
    for (uint32_t i = 0; i < take; i++) {
        dst[i] = g_ready[(g_ready_head + i) % READY_CAP];
    }
    g_ready_head = (g_ready_head + take) % READY_CAP;
    g_ready_len -= take;
    return take;
}

static uint32_t min_read_ready(void) {
    if (g_tio.c_lflag & ICANON) {
        return 1u;
    }
    uint32_t vmin = (uint32_t)g_tio.c_cc[VMIN];
    return vmin == 0u ? 1u : vmin;
}

static int input_is_readable(void) {
    if (g_eof_pending > 0u) {
        return 1;
    }
    return g_ready_len >= min_read_ready();
}

static int16_t eval_poll_events(int16_t events) {
    int16_t rev = 0;
    if (input_is_readable()) {
        rev = (int16_t)(rev | (events & (POLLIN | POLLRDNORM)));
    }
    rev = (int16_t)(rev | (events & (POLLOUT | POLLWRNORM)));
    return rev;
}

static void wake_waiters(void) {
    /* Wake blocked readers first if input is ready. */
    for (uint32_t i = 0; i < RWAIT_MAX; i++) {
        if (!g_rwait[i].used || !input_is_readable()) {
            continue;
        }
        tty_rep_t rep = {0};
        if (g_ready_len > 0u) {
            uint32_t want = g_rwait[i].want_len > TTY_DATA_MAX
                                ? TTY_DATA_MAX
                                : g_rwait[i].want_len;
            rep.len = ready_pop(rep.u.data, want);
        } else if (g_eof_pending > 0u) {
            g_eof_pending--;
            rep.len = 0u;
        }
        rep.err = 0;
        uint32_t msgid = g_rwait[i].msgid;
        g_rwait[i].used = 0u;
        (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
    }

    /* Wake blocked pollers if ready or timed out. */
    uint64_t now = 0;
    for (uint32_t i = 0; i < PWAIT_MAX; i++) {
        if (!g_pwait[i].used) {
            continue;
        }
        int16_t rev = eval_poll_events(g_pwait[i].events);
        int expired = 0;
        if (rev == 0 && g_pwait[i].has_deadline) {
            if (now == 0) {
                now = now_mono_ns();
            }
            if (now >= g_pwait[i].deadline_ns) {
                expired = 1;
            }
        }
        if (rev != 0 || expired) {
            tty_rep_t rep = {0};
            rep.err = 0;
            rep.revents = rev;
            uint32_t msgid = g_pwait[i].msgid;
            if (g_pwait[i].has_deadline) {
                dec_timed_polls();
            }
            g_pwait[i].used = 0u;
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
        }
    }
}

static void commit_edit_buffer(void) {
    for (uint32_t i = 0; i < g_edit_len; i++) {
        ready_push(g_edit[i]);
    }
    g_edit_len = 0u;
}

static void uart_putc_raw(uint8_t c) {
    tty_uart_putc(c);
}

static void tty_tx_byte(uint8_t c) {
    if ((g_tio.c_oflag & OPOST) != 0u && (g_tio.c_oflag & ONLCR) != 0u &&
        c == '\n') {
        uart_putc_raw('\r');
    }
    uart_putc_raw(c);
}

static void tty_feed_byte(uint8_t c, int allow_echo) {
    if (g_tio.c_iflag & ISTRIP) {
        c &= 0x7Fu;
    }
    if (c == '\r') {
        if (g_tio.c_iflag & IGNCR) {
            return;
        }
        if (g_tio.c_iflag & ICRNL) {
            c = '\n';
        }
    } else if (c == '\n' && (g_tio.c_iflag & INLCR)) {
        c = '\r';
    }

    int do_echo = allow_echo && ((g_tio.c_lflag & ECHO) != 0u);

    if (g_tio.c_lflag & ICANON) {
        if (c == g_tio.c_cc[VERASE] || c == '\b') {
            if (g_edit_len > 0u) {
                g_edit_len--;
                if (do_echo && (g_tio.c_lflag & ECHOE) != 0u) {
                    uart_putc_raw('\b');
                    uart_putc_raw(' ');
                    uart_putc_raw('\b');
                }
            }
            return;
        }
        if (c == g_tio.c_cc[VKILL]) {
            if (do_echo && (g_tio.c_lflag & ECHOE) != 0u) {
                while (g_edit_len > 0u) {
                    g_edit_len--;
                    uart_putc_raw('\b');
                    uart_putc_raw(' ');
                    uart_putc_raw('\b');
                }
            } else {
                g_edit_len = 0u;
            }
            return;
        }
        if (c == g_tio.c_cc[VEOF]) {
            if (g_edit_len > 0u) {
                commit_edit_buffer();
            } else {
                g_eof_pending++;
            }
            wake_waiters();
            return;
        }
        if (g_edit_len < EDIT_CAP) {
            g_edit[g_edit_len++] = c;
            if (do_echo) {
                tty_tx_byte(c);
            }
        }
        int is_eol = (c == '\n') ||
                     (g_tio.c_cc[VEOL] != 0u && c == g_tio.c_cc[VEOL]) ||
                     (g_edit_len >= EDIT_CAP);
        if (is_eol) {
            commit_edit_buffer();
            wake_waiters();
        }
    } else {
        ready_push(c);
        if (do_echo) {
            tty_tx_byte(c);
        }
        wake_waiters();
    }
}

static void drain_com1_rx(int feed_ldisc) {
    uint8_t b = 0;
    while (tty_uart_getc(&b)) {
        if (feed_ldisc) {
            tty_feed_byte(b, 1);
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

static void handle_read(uint32_t msgid, const tty_req_t *req) {
    tty_rep_t rep = {0};
    uint32_t want = req->len > TTY_DATA_MAX ? TTY_DATA_MAX : req->len;
    if (want == 0u) {
        (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    if (input_is_readable()) {
        if (g_ready_len > 0u) {
            rep.len = ready_pop(rep.u.data, want);
        } else if (g_eof_pending > 0u) {
            g_eof_pending--;
            rep.len = 0u;
        }
        (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    if ((req->flags & (uint32_t)O_NONBLOCK) != 0u ||
        (!(g_tio.c_lflag & ICANON) && g_tio.c_cc[VMIN] == 0u)) {
        rep.err = (req->flags & (uint32_t)O_NONBLOCK) ? -EAGAIN : 0;
        (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    for (uint32_t i = 0; i < RWAIT_MAX; i++) {
        if (!g_rwait[i].used) {
            g_rwait[i].used = 1u;
            g_rwait[i].msgid = msgid;
            g_rwait[i].want_len = want;
            return;
        }
    }
    rep.err = -EAGAIN;
    (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
}

static void handle_poll(uint32_t msgid, const tty_req_t *req) {
    tty_rep_t rep = {0};
    int16_t events = (int16_t)req->arg;
    int16_t rev = eval_poll_events(events);
    if (rev != 0 || req->timeout_ns == 0) {
        rep.err = 0;
        rep.revents = rev;
        (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    for (uint32_t i = 0; i < PWAIT_MAX; i++) {
        if (!g_pwait[i].used) {
            g_pwait[i].used = 1u;
            g_pwait[i].msgid = msgid;
            g_pwait[i].events = events;
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

    init_default_termios();

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

    tty_req_t req;
    tty_rep_t rep;
    ipc_recv_info_t info;
    for (;;) {
        memset(&req, 0, sizeof(req));
        memset(&rep, 0, sizeof(rep));
        memset(&info, 0, sizeof(info));
        long msgid = ReceiveMsg(chan, &req, sizeof(req), &info);
        if (msgid < 0) {
            continue;
        }
        if (msgid == 0) {
            const ipc_pulse_t *pulse = (const ipc_pulse_t *)(const void *)&req;
            if (pulse->code == PULSE_CODE_IRQ && pulse->value == g_uart_irq) {
                drain_com1_rx(1);
                (void)IntrUnmask(g_uart_irq);
            } else if (pulse->code == PULSE_CODE_TTY_TICK) {
                wake_waiters();
            }
            continue;
        }

        switch (req.op) {
        case TTY_OP_READ:
            handle_read((uint32_t)msgid, &req);
            break;
        case TTY_OP_WRITE: {
            uint32_t len = req.len > TTY_DATA_MAX ? TTY_DATA_MAX : req.len;
            for (uint32_t i = 0; i < len; i++) {
                tty_tx_byte(req.u.data[i]);
            }
            rep.err = 0;
            rep.len = len;
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        }
        case TTY_OP_GETATTR:
            rep.err = 0;
            rep.u.tio = g_tio;
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        case TTY_OP_SETATTR:
            if (req.arg != (uint32_t)TCSANOW &&
                req.arg != (uint32_t)TCSADRAIN &&
                req.arg != (uint32_t)TCSAFLUSH) {
                rep.err = -EINVAL;
            } else {
                if (req.arg == (uint32_t)TCSAFLUSH) {
                    g_edit_len = 0u;
                    g_ready_head = 0u;
                    g_ready_len = 0u;
                    g_eof_pending = 0u;
                }
                g_tio = req.u.tio;
                rep.err = 0;
                wake_waiters();
            }
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        case TTY_OP_FLUSH:
            if (req.arg != (uint32_t)TCIFLUSH &&
                req.arg != (uint32_t)TCOFLUSH &&
                req.arg != (uint32_t)TCIOFLUSH) {
                rep.err = -EINVAL;
            } else {
                if (req.arg == (uint32_t)TCIFLUSH ||
                    req.arg == (uint32_t)TCIOFLUSH) {
                    g_edit_len = 0u;
                    g_ready_head = 0u;
                    g_ready_len = 0u;
                    g_eof_pending = 0u;
                }
                rep.err = 0;
            }
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        case TTY_OP_POLL:
            handle_poll((uint32_t)msgid, &req);
            break;
        case TTY_OP_INJECT: {
            uint32_t len = req.len > TTY_DATA_MAX ? TTY_DATA_MAX : req.len;
            for (uint32_t i = 0; i < len; i++) {
                tty_feed_byte(req.u.data[i], 0);
            }
            rep.err = 0;
            rep.len = len;
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        }
        case TTY_OP_DRAIN:
        case TTY_OP_SENDBREAK:
            rep.err = 0;
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        case TTY_OP_FLOW:
            if (req.arg != (uint32_t)TCOOFF && req.arg != (uint32_t)TCOON &&
                req.arg != (uint32_t)TCIOFF && req.arg != (uint32_t)TCION) {
                rep.err = -EINVAL;
            } else {
                rep.err = 0;
            }
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        default:
            rep.err = -ENOSYS;
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
            break;
        }
    }
}
