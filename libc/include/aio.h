/**
 * @file     aio.h
 * @brief    POSIX.1-2024 Base asynchronous input and output declarations
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _AIO_H
#define _AIO_H

/* need:: ICD_IPC_0006 */

#include <signal.h>
#include <stddef.h>
#include <sys/types.h>
#include <time.h>

/* aio_cancel() return values. */
#define AIO_ALLDONE 0
#define AIO_CANCELED 1
#define AIO_NOTCANCELED 2

/* lio_listio() element operations. */
#define LIO_NOP 0
#define LIO_READ 1
#define LIO_WRITE 2

/* lio_listio() synchronization modes. */
#define LIO_WAIT 0
#define LIO_NOWAIT 1

struct aiocb {
    int aio_fildes;
    off_t aio_offset;
    volatile void *aio_buf;
    size_t aio_nbytes;
    int aio_reqprio;
    struct sigevent aio_sigevent;
    int aio_lio_opcode;

    /* Internal state fields (reserved namespace). */
    int __aio_err;
    ssize_t __aio_ret;
    unsigned int __aio_state;
};

int aio_cancel(int fildes, struct aiocb *aiocbp);
int aio_error(const struct aiocb *aiocbp);
int aio_fsync(int op, struct aiocb *aiocbp);
int aio_read(struct aiocb *aiocbp);
ssize_t aio_return(struct aiocb *aiocbp);
int aio_suspend(const struct aiocb *const list[], int nent,
                const struct timespec *timeout);
int aio_write(struct aiocb *aiocbp);
int lio_listio(int mode, struct aiocb *restrict const list[restrict], int nent,
               struct sigevent *restrict sig);

#endif /* _AIO_H */
