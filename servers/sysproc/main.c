/* sysproc — PID 1: process manager, pathname manager, ELF loader, signals.
 * Milestone 3: the system name registry. Servers ATTACH paths on chanid 1;
 * clients QUERY them. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/microkernel.h>
#include <sys/ipc.h>
#include <sys/bootinfo.h>

#define NAME_TABLE_MAX 8

typedef struct {
    char path[NAME_MAX_PATH];
    int pid;
    int chanid;
} name_entry_t;

static name_entry_t g_names[NAME_TABLE_MAX];
static int g_name_count;

int main(int argc, char **argv, char **envp) {
    printf("[OK] Ring 3 sysproc entered main() (argc=%d argv0=%s)\n", argc,
           argc > 0 ? argv[0] : "?");
    (void)envp;

    int chanid = (int)__sys0(SYS_CHANNEL_CREATE);
    if (chanid < 0) {
        printf("[FAIL] SYS_CHANNEL_CREATE -> %d\n", chanid);
        return 1;
    }
    printf("[OK] sysproc channel created: chanid=%d\n", chanid);

    /* Early console: report the boot environment handed to us. */
    unsigned long *auxv = (unsigned long *)__auxv;
    unsigned long bootinfo_va = 0;
    for (unsigned long i = 0; auxv[i]; i += 2) {
        if (auxv[i] == AT_MICROKERNEL_BOOTINFO) {
            bootinfo_va = auxv[i + 1];
        }
        if (auxv[i] == 0) {
            break;
        }
    }
    if (bootinfo_va) {
        boot_info_t *bi = (boot_info_t *)bootinfo_va;
        if (bi->magic == BOOTINFO_MAGIC) {
            printf("[OK] bootinfo: bootfs @%llx (%llu bytes), RSDP @%llx\n",
                   (unsigned long long)bi->boot_tar_phys,
                   (unsigned long long)bi->boot_tar_size,
                   (unsigned long long)bi->rsdp_phys);
        } else {
            printf("[FAIL] bootinfo magic mismatch\n");
        }
    } else {
        printf("[FAIL] AT_MICROKERNEL_BOOTINFO missing from auxv\n");
    }

    /* Name service: receive loop over our channel. */
    printf("[OK] sysproc name server listening (chanid=%d)\n", chanid);
    name_msg_t msg;
    name_reply_t rep;
    ipc_recv_info_t info;
    int pulses = 0;
    for (;;) {
        long msgid = ReceiveMsg(chanid, &msg, sizeof(msg), &info);
        if (msgid < 0) {
            printf("[FAIL] sysproc ReceiveMsg -> %ld\n", msgid);
            continue;
        }
        if (msgid == 0) {
            pulses++;
            if (pulses == 1) {
                printf("[OK] sysproc received pulse\n");
            }
            continue;
        }

        rep.pid = 0;
        rep.chanid = 0;
        rep.err = 0;
        rep.reserved = 0;
        msg.path[NAME_MAX_PATH - 1] = '\0';

        if (msg.type == NAME_MSG_ATTACH) {
            if (g_name_count >= NAME_TABLE_MAX) {
                rep.err = 1; /* table full */
            } else {
                name_entry_t *e = &g_names[g_name_count++];
                memcpy(e->path, msg.path, NAME_MAX_PATH);
                e->pid = (int)info.pid;
                e->chanid = (int)msg.chanid;
                printf("[sysproc] registered '%s' -> pid=%d chanid=%d\n",
                       e->path, e->pid, e->chanid);
            }
        } else if (msg.type == NAME_MSG_QUERY) {
            int found = 0;
            for (int i = 0; i < g_name_count; i++) {
                if (strcmp(g_names[i].path, msg.path) == 0) {
                    rep.pid = g_names[i].pid;
                    rep.chanid = g_names[i].chanid;
                    found = 1;
                    break;
                }
            }
            if (!found) {
                rep.err = 2; /* not found */
            } else {
                printf("[sysproc] resolved '%s' -> pid=%d chanid=%d\n",
                       msg.path, rep.pid, rep.chanid);
            }
        } else {
            rep.err = 3; /* unknown request */
        }

        if (ReplyMsg((int)msgid, &rep, sizeof(rep)) < 0) {
            printf("[FAIL] sysproc ReplyMsg -> %ld\n", msgid);
        }
    }
}
