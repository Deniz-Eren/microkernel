/**
 * @file     ioctl.h
 * @brief    Terminal and file descriptor ioctl request codes and interface
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H

/* need:: ICD_IPC_0018 */

#include <termios.h>

#define TIOCGWINSZ 0x5413UL
#define TIOCSWINSZ 0x5414UL
#define TIOCSCTTY 0x540EUL
#define TIOCNOTTY 0x5422UL
#define TIOCGPGRP 0x540FUL
#define TIOCSPGRP 0x5410UL
#define FIONREAD 0x541BUL
#define FIONBIO 0x5421UL

int ioctl(int fildes, unsigned long request, ...);

#endif /* _SYS_IOCTL_H */
