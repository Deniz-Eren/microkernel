/**
 * @file     termios.h
 * @brief    POSIX.1-2024 terminal I/O control flags, structures, and functions
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _TERMIOS_H
#define _TERMIOS_H

/* need:: ICD_IPC_0011 ICD_PROC_0009 */

#include <sys/types.h>

typedef unsigned char cc_t;
typedef unsigned int speed_t;
typedef unsigned int tcflag_t;

#define NCCS 16

/* Control character indices in c_cc[]. */
#define VEOF 0
#define VEOL 1
#define VERASE 2
#define VINTR 3
#define VKILL 4
#define VMIN 5
#define VQUIT 6
#define VSTART 7
#define VSTOP 8
#define VSUSP 9
#define VTIME 10

/* Input modes (c_iflag). */
#define IGNBRK 0x0001u
#define BRKINT 0x0002u
#define IGNPAR 0x0004u
#define PARMRK 0x0008u
#define INPCK 0x0010u
#define ISTRIP 0x0020u
#define INLCR 0x0040u
#define IGNCR 0x0080u
#define ICRNL 0x0100u
#define IXON 0x0400u
#define IXANY 0x0800u
#define IXOFF 0x1000u

/* Output modes (c_oflag). */
#define OPOST 0x0001u
#define ONLCR 0x0004u
#define OCRNL 0x0008u
#define ONOCR 0x0010u
#define ONLRET 0x0020u

/* Control modes (c_cflag). */
#define CSIZE 0x0030u
#define CS5 0x0000u
#define CS6 0x0010u
#define CS7 0x0020u
#define CS8 0x0030u
#define CSTOPB 0x0040u
#define CREAD 0x0080u
#define PARENB 0x0100u
#define PARODD 0x0200u
#define HUPCL 0x0400u
#define CLOCAL 0x0800u

/* Local modes (c_lflag). */
#define ISIG 0x0001u
#define ICANON 0x0002u
#define ECHO 0x0008u
#define ECHOE 0x0010u
#define ECHOK 0x0020u
#define ECHONL 0x0040u
#define NOFLSH 0x0080u
#define TOSTOP 0x0100u
#define IEXTEN 0x8000u

/* Baud rate constants. */
#define B0 0u
#define B50 50u
#define B75 75u
#define B110 110u
#define B134 134u
#define B150 150u
#define B200 200u
#define B300 300u
#define B600 600u
#define B1200 1200u
#define B1800 1800u
#define B2400 2400u
#define B4800 4800u
#define B9600 9600u
#define B19200 19200u
#define B38400 38400u
#define B57600 57600u
#define B115200 115200u

/* Optional actions for tcsetattr(). */
#define TCSANOW 0
#define TCSADRAIN 1
#define TCSAFLUSH 2

/* Queue selectors for tcflush(). */
#define TCIFLUSH 0
#define TCOFLUSH 1
#define TCIOFLUSH 2

/* Actions for tcflow(). */
#define TCOOFF 0
#define TCOON 1
#define TCIOFF 2
#define TCION 3

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_cc[NCCS];
    speed_t __c_ispeed;
    speed_t __c_ospeed;
};

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
};

speed_t cfgetispeed(const struct termios *termios_p);
speed_t cfgetospeed(const struct termios *termios_p);
int cfsetispeed(struct termios *termios_p, speed_t speed);
int cfsetospeed(struct termios *termios_p, speed_t speed);
int tcdrain(int fildes);
int tcflow(int fildes, int action);
int tcflush(int fildes, int queue_selector);
int tcgetattr(int fildes, struct termios *termios_p);
pid_t tcgetsid(int fildes);
int tcgetwinsize(int fildes, struct winsize *winsize_p);
int tcsendbreak(int fildes, int duration);
int tcsetattr(int fildes, int optional_actions,
              const struct termios *termios_p);
int tcsetwinsize(int fildes, const struct winsize *winsize_p);

#endif /* _TERMIOS_H */
