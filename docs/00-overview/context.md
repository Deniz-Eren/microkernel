# System Context

`microkernel` is a small x86-64 operating system. The Ring 0 kernel runs at the
higher-half virtual address `0xFFFFFFFF80000000` and supplies five services:
boot and platform bring-up, W^X memory management, preemptive thread scheduling,
synchronous IPC, and Ring 3 signal delivery.

System servers (`sysproc`, `ipcserv`) and user applications run in Ring 3 and
communicate through kernel channels and a freestanding C17 library (`libc`).
GRUB boots the kernel ISO with the Multiboot2 protocol, and headless QEMU
verifies the serial output of each milestone.
