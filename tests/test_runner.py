##
# @file     test_runner.py
# @brief    Headless QEMU serial-output assertion harness for the boot ISO
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

# need:: VER_BOOT_0001 VER_MEM_0001 VER_MEM_0002 VER_MEM_0003
# need:: VER_PROC_0001 VER_PROC_0002 VER_PROC_0003 VER_PROC_0004 VER_PROC_0005
# need:: VER_IPC_0001 VER_IPC_0002 VER_IPC_0003 VER_IPC_0004 VER_IPC_0005
# need:: VER_SIG_0001 VER_SIG_0002 VER_LIBC_0001 VER_LIBC_0003 VER_LIBC_0004
# need:: VER_LIBC_0005
"""Headless QEMU serial-output assertion harness for Microkernel.

Boots build/microkernel.iso with TCG software emulation (no KVM required), pipes
the serial console to stdout, and asserts that an ordered list of regex patterns
appears on the serial line. Exits 0 on success.

Usage:
    test_runner.py [--iso build/microkernel.iso] [--timeout 120]
                   --expect 'regex' [--expect 'regex' ...] [--extra-qemu-arg
                   '-drive file=build/disk.img,if=none,id=nvm' ...]
"""
import argparse
import re
import shlex
import signal
import subprocess
import sys
import threading
import time


def build_cmd(args):
    cmd = shlex.split(args.qemu) + [
        "-machine", "q35",
        "-cpu", "qemu64,+smap,+smep",
        "-m", "256M",
        "-smp", "2",
        "-cdrom", args.iso,
        "-serial", "stdio",
        "-display", "none",
        "-no-reboot",
        "-no-shutdown",
        "-monitor", "none",
    ]
    for extra in args.extra_qemu_arg:
        cmd.extend(shlex.split(extra))
    return cmd


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--iso", default="build/microkernel.iso")
    ap.add_argument("--timeout", type=float, default=120.0)
    ap.add_argument("--expect", action="append", default=[])
    ap.add_argument("--extra-qemu-arg", action="append", default=[])
    ap.add_argument("--qemu", default="qemu-system-x86_64")
    args = ap.parse_args()

    if not args.expect:
        print("test_runner: no --expect patterns given", file=sys.stderr)
        return 2

    patterns = [re.compile(p) for p in args.expect]
    cmd = build_cmd(args)
    print(f"[test_runner] {' '.join(cmd)}")

    proc = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        stdin=subprocess.DEVNULL, text=True, errors="replace",
    )

    matched = [False] * len(patterns)
    success = threading.Event()
    tail = []

    def reader():
        try:
            for line in proc.stdout:
                sys.stdout.write(line)
                sys.stdout.flush()
                tail.append(line)
                del tail[:-200]
                for i, pat in enumerate(patterns):
                    if not matched[i] and pat.search(line):
                        matched[i] = True
                        if all(matched):
                            success.set()
        except (ValueError, OSError):
            pass

    t = threading.Thread(target=reader, daemon=True)
    t.start()

    deadline = time.time() + args.timeout
    result = None
    while time.time() < deadline:
        if success.is_set():
            result = "PASS"
            break
        if proc.poll() is not None:
            result = "FAIL (qemu exited early, rc=%s)" % proc.returncode
            break
        time.sleep(0.05)

    if result is None:
        result = "FAIL (timeout after %.0fs)" % args.timeout

    if success.is_set():
        proc.send_signal(signal.SIGTERM)
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()

    missing = [args.expect[i] for i, m in enumerate(matched) if not m]
    if success.is_set():
        print(f"[test_runner] PASS: all {len(patterns)} expectations matched")
        return 0
    print(f"[test_runner] {result}")
    print(f"[test_runner] missing expectations: {missing}")
    print("[test_runner] --- last serial output ---")
    for line in tail[-60:]:
        sys.stdout.write("    " + line)
    return 1


if __name__ == "__main__":
    sys.exit(main())
