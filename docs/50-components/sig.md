# Components — SIG

Domain `SIG`: White-box specification for kernel signal delivery and
restoration.

```{comp} Kernel signal delivery and sigreturn handler
:id: COMP_SIG_0001
:status: active
:refines: ARC_SIG_0001, ICD_SIG_0001
:satisfies: SWR_SIG_0001, SWR_SIG_0002

`kernel/core/signal.c` implements `sys_sigaction`, `sys_inject_sig`,
`signal_deliver_check`, and `sys_sigreturn`, pushing `sig_frame_t` onto the user
stack via `copy_to_user` ({need}`ICD_SIG_0001`) and terminating processes on
unhandled signals for the signal subsystem ({need}`ARC_SIG_0001`).
```
