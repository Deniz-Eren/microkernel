# Interfaces — SIG

Domain `SIG`: Signal numbers, user wrappers, and signal frame layout.

```{icd} Ring 3 signal ABI and sig_frame_t layout
:id: ICD_SIG_0001
:status: active
:refines: ARC_SIG_0001
:satisfies: SWR_SIG_0001, SWR_SIG_0002

`libc/include/sys/signal.h` and `kernel/include/kernel/signal.h` define signal
constants, `sigaction`, `kill`, `__sigreturn_trampoline`, and the 16-byte
aligned `sig_frame_t` stack frame for the signal subsystem
({need}`ARC_SIG_0001`), satisfying registration/injection ({need}`SWR_SIG_0001`)
and frame delivery/restoration ({need}`SWR_SIG_0002`).
```
