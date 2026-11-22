/**
 * @file     uname.c
 * @brief    Ring 3 uname utility targeting POSIX.1-2024 XCU (-a/-s/-n/-r/-v/-m)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdio.h>
#include <sys/utsname.h>
#include <unistd.h>

/* need:: COMP_PROC_0007 */

int main(int argc, char **argv) {
    unsigned int mask = 0u;
    int opt;
    while ((opt = getopt(argc, argv, "amnrsv")) != -1) {
        switch (opt) {
        case 'a':
            mask = 0x1Fu;
            break;
        case 's':
            mask |= 0x01u;
            break;
        case 'n':
            mask |= 0x02u;
            break;
        case 'r':
            mask |= 0x04u;
            break;
        case 'v':
            mask |= 0x08u;
            break;
        case 'm':
            mask |= 0x10u;
            break;
        default:
            return 1;
        }
    }
    if (mask == 0u) {
        mask = 0x01u;
    }
    struct utsname u;
    if (uname(&u) != 0) {
        perror("uname");
        return 1;
    }
    const char *fields[5] = {u.sysname, u.nodename, u.release, u.version,
                             u.machine};
    int printed = 0;
    for (int i = 0; i < 5; i++) {
        if (mask & (1u << (unsigned int)i)) {
            if (printed) {
                (void)putchar(' ');
            }
            (void)fputs(fields[i], stdout);
            printed = 1;
        }
    }
    (void)putchar('\n');
    return 0;
}
