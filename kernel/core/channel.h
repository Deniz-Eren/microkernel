#pragma once
#include <stdint.h>
#include <kernel/ipc.h>

/* Kernel-owned channel objects (synchronous IPC endpoints). */
#define MAX_CHANNELS 256

typedef struct channel {
    uint32_t used;
    uint32_t owner_pid;
    uint32_t chanid;

    /* Pending messages, priority-ordered (highest effective priority first,
     * FIFO within a level). */
    chan_msg_t *msg_head;
    chan_msg_t *msg_tail;

    /* Threads blocked in ReceiveMsg, priority-ordered. */
    struct tcb *recv_head;
    struct tcb *recv_tail;

    /* Async pulses, priority-ordered. */
    chan_msg_t *pulse_head;
    chan_msg_t *pulse_tail;

    uint32_t refcount;
} channel_t;

int channel_create(uint32_t owner_pid);
int channel_destroy(uint32_t owner_pid, int chanid);
channel_t *channel_get(int chanid);

/* Priority-ordered queue helpers (implemented in channel.c). */
void channel_msg_enqueue(channel_t *ch, chan_msg_t *m);
chan_msg_t *channel_msg_dequeue(channel_t *ch);
void channel_msg_remove(channel_t *ch, chan_msg_t *m);

void channel_pulse_enqueue(channel_t *ch, chan_msg_t *m);
chan_msg_t *channel_pulse_dequeue(channel_t *ch);

void channel_recv_enqueue(channel_t *ch, struct tcb *t);
struct tcb *channel_recv_dequeue(channel_t *ch);
void channel_recv_remove(channel_t *ch, struct tcb *t);
