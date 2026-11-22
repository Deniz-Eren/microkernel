/**
 * @file     stty.c
 * @brief    POSIX.1-2024 XCU stty utility (set options for a terminal)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/* need:: COMP_PROC_0009 */

typedef struct {
    const char *name;
    tcflag_t mask;
    int field; /* 0 = iflag, 1 = oflag, 2 = cflag, 3 = lflag */
} stty_flag_t;

static const stty_flag_t k_flags[] = {
    {"ignbrk", IGNBRK, 0}, {"brkint", BRKINT, 0}, {"ignpar", IGNPAR, 0},
    {"parmrk", PARMRK, 0}, {"inpck", INPCK, 0},   {"istrip", ISTRIP, 0},
    {"inlcr", INLCR, 0},   {"igncr", IGNCR, 0},   {"icrnl", ICRNL, 0},
    {"ixon", IXON, 0},     {"ixany", IXANY, 0},   {"ixoff", IXOFF, 0},
    {"opost", OPOST, 1},   {"onlcr", ONLCR, 1},   {"ocrnl", OCRNL, 1},
    {"onocr", ONOCR, 1},   {"onlret", ONLRET, 1}, {"cstopb", CSTOPB, 2},
    {"cread", CREAD, 2},   {"parenb", PARENB, 2}, {"parodd", PARODD, 2},
    {"hupcl", HUPCL, 2},   {"clocal", CLOCAL, 2}, {"isig", ISIG, 3},
    {"icanon", ICANON, 3}, {"echo", ECHO, 3},     {"echoe", ECHOE, 3},
    {"echok", ECHOK, 3},   {"echonl", ECHONL, 3}, {"noflsh", NOFLSH, 3},
    {"tostop", TOSTOP, 3}, {"iexten", IEXTEN, 3},
};

static void format_cc(cc_t ch, char *buf, size_t cap) {
    if (ch == 0u) {
        (void)strlcpy(buf, "<undef>", cap);
    } else if (ch == 0x7Fu) {
        (void)strlcpy(buf, "^?", cap);
    } else if (ch < 0x20u) {
        (void)snprintf(buf, cap, "^%c", (char)(ch + '@'));
    } else {
        (void)snprintf(buf, cap, "%c", (char)ch);
    }
}

static void print_all(const struct termios *tio, const struct winsize *ws) {
    char c_intr[12], c_quit[12], c_erase[12], c_kill[12], c_eof[12];
    format_cc(tio->c_cc[VINTR], c_intr, sizeof(c_intr));
    format_cc(tio->c_cc[VQUIT], c_quit, sizeof(c_quit));
    format_cc(tio->c_cc[VERASE], c_erase, sizeof(c_erase));
    format_cc(tio->c_cc[VKILL], c_kill, sizeof(c_kill));
    format_cc(tio->c_cc[VEOF], c_eof, sizeof(c_eof));

    (void)printf("speed %u baud; rows %u; columns %u;\n",
                 (unsigned)cfgetospeed(tio), (unsigned)ws->ws_row,
                 (unsigned)ws->ws_col);
    (void)printf("intr = %s; quit = %s; erase = %s; kill = %s; eof = %s;\n",
                 c_intr, c_quit, c_erase, c_kill, c_eof);
    (void)printf(
        "%sicrnl %sixon %sopost %sonlcr\n", (tio->c_iflag & ICRNL) ? "" : "-",
        (tio->c_iflag & IXON) ? "" : "-", (tio->c_oflag & OPOST) ? "" : "-",
        (tio->c_oflag & ONLCR) ? "" : "-");
    (void)printf(
        "%sisig %sicanon %secho %sechoe %sechok %snoflsh %stostop\n",
        (tio->c_lflag & ISIG) ? "" : "-", (tio->c_lflag & ICANON) ? "" : "-",
        (tio->c_lflag & ECHO) ? "" : "-", (tio->c_lflag & ECHOE) ? "" : "-",
        (tio->c_lflag & ECHOK) ? "" : "-", (tio->c_lflag & NOFLSH) ? "" : "-",
        (tio->c_lflag & TOSTOP) ? "" : "-");
}

static void print_saved(const struct termios *tio) {
    (void)printf("%x:%x:%x:%x", (unsigned)tio->c_iflag, (unsigned)tio->c_oflag,
                 (unsigned)tio->c_cflag, (unsigned)tio->c_lflag);
    for (int i = 0; i < NCCS; i++) {
        (void)printf(":%x", (unsigned)tio->c_cc[i]);
    }
    (void)putchar('\n');
}

static void apply_sane(struct termios *tio) {
    tio->c_iflag = ICRNL | IXON;
    tio->c_oflag = OPOST | ONLCR;
    tio->c_cflag = CS8 | CREAD | CLOCAL;
    tio->c_lflag = ICANON | ECHO | ECHOE | ECHOK | ISIG;
    tio->c_cc[VEOF] = 0x04u;
    tio->c_cc[VERASE] = 0x7Fu;
    tio->c_cc[VINTR] = 0x03u;
    tio->c_cc[VKILL] = 0x15u;
    tio->c_cc[VQUIT] = 0x1Cu;
    tio->c_cc[VMIN] = 1u;
    tio->c_cc[VTIME] = 0u;
}

static int apply_flag(struct termios *tio, const char *tok) {
    int neg = 0;
    if (tok[0] == '-') {
        neg = 1;
        tok++;
    }
    for (size_t i = 0; i < sizeof(k_flags) / sizeof(k_flags[0]); i++) {
        if (strcmp(k_flags[i].name, tok) == 0) {
            tcflag_t *dst = NULL;
            if (k_flags[i].field == 0) {
                dst = &tio->c_iflag;
            } else if (k_flags[i].field == 1) {
                dst = &tio->c_oflag;
            } else if (k_flags[i].field == 2) {
                dst = &tio->c_cflag;
            } else {
                dst = &tio->c_lflag;
            }
            if (neg) {
                *dst &= ~k_flags[i].mask;
            } else {
                *dst |= k_flags[i].mask;
            }
            return 0;
        }
    }
    return -1;
}

static int parse_u16(const char *s, unsigned short *out) {
    char *end = NULL;
    errno = 0;
    unsigned long v = strtoul(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || v > 65535UL) {
        return -1;
    }
    *out = (unsigned short)v;
    return 0;
}

int main(int argc, char **argv) {
    int opt_a = 0;
    int opt_g = 0;
    int idx = 1;

    while (idx < argc && argv[idx][0] == '-' && argv[idx][1] != '\0') {
        if (strcmp(argv[idx], "--") == 0) {
            idx++;
            break;
        }
        if (strcmp(argv[idx], "-a") == 0) {
            opt_a = 1;
            idx++;
        } else if (strcmp(argv[idx], "-g") == 0) {
            opt_g = 1;
            idx++;
        } else {
            /* Could be a negated stty mode like -echo or -icanon. */
            break;
        }
    }

    if ((opt_a && opt_g) || ((opt_a || opt_g) && idx < argc)) {
        (void)fprintf(stderr,
                      "stty: -a and -g cannot be combined with modes\n");
        return 1;
    }

    struct termios tio;
    if (tcgetattr(STDIN_FILENO, &tio) != 0) {
        (void)fprintf(stderr, "stty: standard input is not a terminal\n");
        return 1;
    }
    struct winsize ws = {.ws_row = 24u, .ws_col = 80u};
    (void)tcgetwinsize(STDIN_FILENO, &ws);

    if (opt_a) {
        print_all(&tio, &ws);
        return 0;
    }
    if (opt_g) {
        print_saved(&tio);
        return 0;
    }
    if (idx >= argc) {
        (void)printf(
            "speed %u baud; %sicanon %secho %sisig\n",
            (unsigned)cfgetospeed(&tio), (tio.c_lflag & ICANON) ? "" : "-",
            (tio.c_lflag & ECHO) ? "" : "-", (tio.c_lflag & ISIG) ? "" : "-");
        return 0;
    }

    int changed_tio = 0;
    int changed_ws = 0;
    for (; idx < argc; idx++) {
        const char *op = argv[idx];
        if (strcmp(op, "sane") == 0 || strcmp(op, "cooked") == 0 ||
            strcmp(op, "-raw") == 0) {
            apply_sane(&tio);
            changed_tio = 1;
        } else if (strcmp(op, "raw") == 0) {
            tio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR |
                             ICRNL | IXON);
            tio.c_oflag &= ~OPOST;
            tio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
            tio.c_cc[VMIN] = 1u;
            tio.c_cc[VTIME] = 0u;
            changed_tio = 1;
        } else if (strcmp(op, "rows") == 0 && idx + 1 < argc) {
            if (parse_u16(argv[++idx], &ws.ws_row) != 0) {
                (void)fprintf(stderr, "stty: invalid rows value\n");
                return 1;
            }
            changed_ws = 1;
        } else if ((strcmp(op, "cols") == 0 || strcmp(op, "columns") == 0) &&
                   idx + 1 < argc) {
            if (parse_u16(argv[++idx], &ws.ws_col) != 0) {
                (void)fprintf(stderr, "stty: invalid columns value\n");
                return 1;
            }
            changed_ws = 1;
        } else if (apply_flag(&tio, op) == 0) {
            changed_tio = 1;
        } else {
            (void)fprintf(stderr, "stty: unknown mode: %s\n", op);
            return 1;
        }
    }

    if (changed_tio && tcsetattr(STDIN_FILENO, TCSANOW, &tio) != 0) {
        (void)fprintf(stderr, "stty: tcsetattr failed\n");
        return 1;
    }
    if (changed_ws && tcsetwinsize(STDIN_FILENO, &ws) != 0) {
        (void)fprintf(stderr, "stty: tcsetwinsize failed\n");
        return 1;
    }
    return 0;
}
