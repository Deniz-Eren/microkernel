/* Microkernel user-space C runtime entry.
 * The kernel builds the initial stack: [argc][argv...][NULL][envp...][NULL]
 * [auxv pairs...][AT_NULL]. RSP points at argc on entry. */

.section .text
.globl _start
.type _start, @function
_start:
    xor %ebp, %ebp                  /* mark outermost frame */
    mov %rsp, %rdi                  /* arg: pointer to argc block */
    and $-16, %rsp                  /* align stack per SysV ABI */
    call __libc_start_main
.Lhang:
    hlt
    jmp .Lhang
.size _start, . - _start

/* Signal return trampoline: the kernel sets the handler's return address
 * here. On entry RSP points at the kernel-built sig_frame_t (the handler's
 * ret popped the trampoline address off the stack). */
.globl __sigreturn_trampoline
.type __sigreturn_trampoline, @function
__sigreturn_trampoline:
    mov %rsp, %rdi                  /* arg: sig_frame_t address */
    mov $23, %eax                   /* SYS_SIGRETURN */
    syscall
    hlt
    jmp __sigreturn_trampoline
.size __sigreturn_trampoline, . - __sigreturn_trampoline
