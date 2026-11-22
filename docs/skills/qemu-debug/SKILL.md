---
name: qemu-debug
description: >-
  Diagnose headless QEMU test failures, kernel exceptions, SMP deadlocks, and
  hung Ring 3 tests using serial logs, the QEMU monitor, and LLVM symbol tools.
  Use when a QEMU test fails, panics, or times out.
---

# QEMU Debugging

Use this runbook when `./scripts/build-agent.sh` or `./scripts/env.sh test`
fails in the QEMU test step. For harness architecture details, see
[docs/60-verification/qemu-harness.md](../../60-verification/qemu-harness.md).

## 1. Inspect the serial log first

`./scripts/build-agent.sh` prints the path to `$TEST_LOG` on failure. Check that
log before changing any code:

1. Search for `!!! kernel exception`, `PANIC:`, and `[FAIL]` lines.
2. Check the `missing expectations:` list printed by `tests/test_runner.py`.
3. Compare spawned processes against zombie processes to see which PIDs stayed
   alive:

```bash
python3 - "$TEST_LOG" << 'PY'
import re, sys
log = open(sys.argv[1]).read()
sp = re.findall(r"spawned '([^']+)' pid=(\d+)", log)
zm = re.findall(r"pid (\d+) became zombie \(exit (-?\d+)\)", log)
spawned = {int(p): n for n, p in sp}
zombies = {int(p): c for p, c in zm}
for p in sorted(spawned):
    if p not in zombies:
        print(f"ALIVE: pid={p} {spawned[p]}")
    elif zombies[p] != "0":
        print(f"NONZERO: pid={p} {spawned[p]} exit={zombies[p]}")
PY
```

Notice if timer-dependent tests (`alarm`, `nanosleep`, `ppoll`) and hardware
driver tests (`blktest`, `rtcdrv`) all stall together. In this kernel, CPU 0
(BSP) runs `time_tick()` and receives all IOAPIC IRQs. A combined timer and IRQ
stall usually means CPU 0 is stuck with interrupts off (`IF=0`) or spinning on a
kernel lock.

## 2. Reproduce with a short timeout

Run a fast incremental build and a short QEMU timeout inside the container:

```bash
./scripts/env.sh build && \
    ./scripts/env.sh test --timeout 30 --expect-file scripts/qemu-expects.txt
```

## 3. Capture live CPU state with the QEMU monitor

When QEMU hangs with no panic output, boot the ISO with a Unix monitor socket
and query both CPUs without editing kernel code:

```bash
./scripts/env.sh bash -c '
python3 - << "PY"
import os, socket, subprocess, time

mon = "/tmp/qmon.sock"
if os.path.exists(mon):
    os.unlink(mon)
p = subprocess.Popen([
    "qemu-system-x86_64", "-machine", "q35", "-m", "256M", "-smp", "2",
    "-cpu", "max", "-cdrom", "build/microkernel.iso",
    "-serial", "file:/tmp/qserial.log", "-display", "none",
    "-no-reboot", "-no-shutdown", "-monitor", f"unix:{mon},server,nowait",
    "-drive", "id=hd0,file=/workspace/build/test-blk.img,if=none,format=raw",
    "-device", "virtio-blk-pci,drive=hd0,disable-legacy=on",
])
time.sleep(10)
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(mon)
s.settimeout(1.0)

def cmd(c):
    s.sendall((c + "\n").encode())
    time.sleep(0.2)
    out = b""
    try:
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            out += chunk
    except socket.timeout:
        pass
    return out.decode(errors="replace")

cmd("")
print(cmd("info cpus"))
print(cmd("cpu 0\ninfo registers"))
print(cmd("cpu 1\ninfo registers"))
p.terminate()
p.wait()
PY
tail -20 /tmp/qserial.log
'
```

Inspect these register fields on each CPU:

- **`RIP`:** Current instruction pointer (`0xFFFFFFFF80...` is Ring 0 kernel
  code, and low canonical addresses are Ring 3 user code).
- **`RFL` (`RFLAGS`):** Bit 9 (`0x200`) is the interrupt flag (`IF`). `IF=0`
  with `HLT=0` in Ring 0 indicates a spinlock wait, lock deadlock, or infinite
  kernel loop. `HLT=1` indicates an idle CPU.
- **`CR3` and `FS`:** Address space and TLS base of the running thread.
- **`RDI`, `RSI`, `RAX`, `RCX`, `RDX`:** Function arguments and ticket spinlock
  counters (`next` and `serving`).

## 4. Symbolize addresses against `build/kernel.elf`

Map any hung `RIP` or exception address back to functions, disassembly, and
global lock or table symbols:

```bash
./scripts/env.sh bash -c '
llvm-addr2line -e build/kernel.elf -f -i 0xffffffff800107f2 0xffffffff8000f000
llvm-objdump -d --start-address=0xffffffff800107c0 \
    --stop-address=0xffffffff80010820 build/kernel.elf
llvm-nm -n build/kernel.elf | \
    grep -E "g_ipc_lock|g_channels|g_proc_lock|g_sched_lock"
'
```

Check lock acquisition order against `ADR_0027` (`g_proc_lock` -> `g_ipc_lock`
-> `g_sched_lock`, and `g_proc_lock` -> `g_futex_lock` -> `g_sched_lock`). Look
for self-loops in linked lists (`RAX == RCX == RSI`) or ticket spinlock waiters
(`pause` loop reading `serving`).

## 5. Isolate SMP races with temporary invariant traps

If the monitor shows corrupted kernel state created earlier in the run:

1. Add a narrow `printk` guard at the invariant boundary (for example, checking
   for duplicate ready-queue entries or unexpected state transitions) and print
   `pid`, `tid`, `state`, and `__builtin_return_address(0)`.
2. Build and run a short `timeout 12 qemu-system-x86_64 ...` capture piped to
   `grep -E '\[BUG\]|\[FAIL\]'`.
3. Symbolize the printed return address with `llvm-addr2line -e
   build/kernel.elf`.
4. Fix the root cause and remove every temporary `printk` line before running
   `./scripts/formatter-agent.sh --fix` and `./scripts/build-agent.sh`.
