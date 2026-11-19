#include <unistd.h>
#include <sys/microkernel.h>

/* Until a console server owns fd 0/1/2, output goes to the kernel early
 * debug log (SYS_DEBUG_LOG). Milestone 4 reroutes this through IPC. */
ssize_t write(int fd, const void *buf, size_t n) {
    if (fd >= 0 && fd <= 2) {
        long r = __sys2(SYS_DEBUG_LOG, (long)buf, (long)n);
        return r < 0 ? -1 : (ssize_t)r;
    }
    return -1;
}

ssize_t read(int fd, void *buf, size_t n) {
    (void)fd;
    (void)buf;
    (void)n;
    return -1;
}

void _exit(int status) {
    __sys1(SYS_PROC_EXIT, status);
    for (;;) {
    }
}

unsigned int sleep(unsigned int seconds) {
    (void)seconds;
    return 0;
}
