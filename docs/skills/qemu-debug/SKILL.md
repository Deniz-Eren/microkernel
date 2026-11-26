---
name: qemu-debug
description: >-
  Find the cause of QEMU test errors, kernel faults, SMP deadlocks, and stalled
  Ring 3 tests. Use serial output, the QEMU monitor, and LLVM symbol tools. Use
  this skill when a QEMU test stops with an error, panics, or times out.
---

# QEMU Debug

Use this procedure when `./scripts/build-agent.sh` or `./scripts/env.sh test`
stops with an error in the QEMU test step. For harness architecture information,
refer to
[docs/60-verification/qemu-harness.md](../../60-verification/qemu-harness.md).

## 1. Examine the serial output first

`./scripts/build-agent.sh` writes the path to `$TEST_LOG` on error. Examine that
output file before you change code:

1. Find `!!! kernel exception`, `PANIC:`, and `[FAIL]` lines.
2. Examine the `missing expectations:` lines that `tests/test_runner.py` writes.
3. Compare started processes with zombie processes to find which PIDs stayed
   active:

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

Timer-dependent tests (`alarm`, `nanosleep`, `ppoll`) and hardware driver tests
(`blktest`, `rtcdrv`) can stall together. In this kernel, CPU 0 (BSP) operates
`time_tick()` and receives all IOAPIC IRQs. A combined timer and IRQ stall
usually shows that CPU 0 is stopped with interrupts off (`IF=0`) or in a kernel
spinlock wait.

## 2. Reproduce with a short timeout

Start an incremental build and a short QEMU timeout in the container:

```bash
./scripts/env.sh build && \
    ./scripts/env.sh test --timeout 30 --expect-file scripts/qemu-expects.txt
```

## 3. Capture live CPU state with the QEMU monitor

If QEMU hangs with no panic output, boot the ISO with a Unix monitor socket and
query the two CPUs without kernel code changes:

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

Examine these register fields on each CPU:

- **`RIP`:** Active instruction pointer (`0xFFFFFFFF80...` is Ring 0 kernel
  code, and low canonical addresses are Ring 3 user code).
- **`RFL` (`RFLAGS`):** Bit 9 (`0x200`) is the interrupt flag (`IF`). `IF=0`
  with `HLT=0` in Ring 0 shows a spinlock wait, lock deadlock, or infinite
  kernel loop. `HLT=1` shows a CPU that is not active.
- **`CR3` and `FS`:** Address space and TLS base of the active thread.
- **`RDI`, `RSI`, `RAX`, `RCX`, `RDX`:** Function arguments and ticket spinlock
  counters (`next` and `serving`).

## 4. Map addresses to `build/kernel.elf`

Map a stalled `RIP` or fault address to functions, disassembly, and global lock
or table symbols:

```bash
./scripts/env.sh bash -c '
llvm-addr2line -e build/kernel.elf -f -i 0xffffffff800107f2 0xffffffff8000f000
llvm-objdump -d --start-address=0xffffffff800107c0 \
    --stop-address=0xffffffff80010820 build/kernel.elf
llvm-nm -n build/kernel.elf | \
    grep -E "g_ipc_lock|g_channels|g_proc_lock|g_sched_lock"
'
```

Compare the lock acquisition sequence with `ADR_0027` (`g_proc_lock` ->
`g_ipc_lock` -> `g_sched_lock`, and `g_proc_lock` -> `g_futex_lock` ->
`g_sched_lock`). Examine the disassembly for self-loops in kernel chains (`RAX
== RCX == RSI`) or ticket spinlock waiters (`pause` loop that reads `serving`).

## 5. Isolate SMP races with temporary rule traps

If the monitor shows corrupt kernel state from a previous step:

1. Add a narrow `printk` check at the rule boundary (for example, duplicate
   entries in the ready queues or incorrect state changes). Write `pid`, `tid`,
   `state`, and `__builtin_return_address(0)`.
2. Compile the kernel and start a short `timeout 12 qemu-system-x86_64 ...`
   capture piped to `grep -E '\[BUG\]|\[FAIL\]'`.
3. Map the output caller address to source lines with `llvm-addr2line -e
   build/kernel.elf`.
4. Repair the root cause and remove all temporary `printk` lines before you
   start `./scripts/formatter-agent.sh --fix` and `./scripts/build-agent.sh`.
