#include <stdarg.h>
#include <kernel/printk.h>
#include <kernel/panic.h>

void panic(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    printk_v(fmt, ap);
    va_end(ap);
    printk("\n!!! panic — system halted\n");
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}
