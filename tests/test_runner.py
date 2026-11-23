##
# @file     test_runner.py
# @brief    Headless QEMU serial-output assertion harness for the boot ISO
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

# need:: VER_BOOT_0001 VER_BOOT_0002 VER_BOOT_0003 VER_BOOT_0004
# need:: VER_MEM_0001 VER_MEM_0002 VER_MEM_0003 VER_MEM_0004
# need:: VER_PROC_0001 VER_PROC_0002 VER_PROC_0003 VER_PROC_0004
# need:: VER_PROC_0005 VER_PROC_0006 VER_PROC_0007 VER_IPC_0001 VER_IPC_0002
# need:: VER_IPC_0003 VER_IPC_0004 VER_IPC_0005 VER_IPC_0006 VER_IPC_0007
# need:: VER_IPC_0008 VER_IPC_0009 VER_IPC_0011 VER_IPC_0012 VER_IPC_0013
# need:: VER_SIG_0001 VER_SIG_0002 VER_LIBC_0001 VER_LIBC_0003 VER_LIBC_0004
# need:: VER_LIBC_0005 VER_LIBC_0006 VER_LIBC_0007 VER_LIBC_0008 VER_LIBC_0009
# need:: VER_LIBC_0010 VER_LIBC_0011 VER_LIBC_0012 VER_LIBC_0013 VER_LIBC_0014
# need:: VER_LIBC_0015 VER_LIBC_0016 VER_LIBC_0017 VER_LIBC_0018 VER_LIBC_0019
# need:: VER_LIBC_0020 VER_LIBC_0021 VER_LIBC_0022 VER_LIBC_0023 VER_LIBC_0024
# need:: VER_LIBC_0025 VER_LIBC_0026 VER_LIBC_0027 VER_LIBC_0028 VER_LIBC_0029
# need:: VER_DOCS_0005 VER_DOCS_0006
"""Headless QEMU serial-output assertion harness for Microkernel.

Boots build/microkernel.iso with TCG software emulation (no KVM required), pipes
the serial console to stdout, and asserts that an ordered list of regex patterns
appears on the serial line. Exits 0 on success.

Usage:
    test_runner.py [--iso build/microkernel.iso] [--timeout 120]
                   --expect 'regex' [--expect-file path] [--cov-out dir
                   --wait-cov] [--extra-qemu-arg '...']
"""
import argparse
from pathlib import Path
import re
import shlex
import signal
import subprocess
import sys
import threading
import time

_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_ROOT / "tools" / "coverage"))
from generate_report import CoverageStreamDecoder  # noqa: E402


ARCH_QEMU_PROFILES = {
    "x86-64": {
        "qemu": "qemu-system-x86_64",
        "cpu": "max",
        "flags": [
            "-machine", "q35",
            "-m", "256M",
        ],
    },
}


def build_cmd(args):
    profile = ARCH_QEMU_PROFILES.get(args.arch)
    if profile is None:
        raise SystemExit(f"test_runner: unsupported --arch '{args.arch}'")
    qemu_bin = args.qemu or profile["qemu"]
    cpu_model = args.cpu or profile["cpu"]
    cmd = (
        shlex.split(qemu_bin)
        + list(profile["flags"])
        + ["-smp", str(args.smp), "-cpu", cpu_model]
        + [
            "-cdrom", args.iso,
            "-serial", "stdio",
            "-display", "none",
            "-no-reboot",
            "-no-shutdown",
            "-monitor", "none",
        ]
    )
    if args.arch == "x86-64" and args.blk_img != "none":
        blk_path = Path(args.blk_img)
        if not blk_path.exists():
            blk_path.parent.mkdir(parents=True, exist_ok=True)
            blk_path.write_bytes(b"\x00" * (2 * 1024 * 1024))
        cmd.extend([
            "-drive", f"id=hd0,file={blk_path},if=none,format=raw",
            "-device", "virtio-blk-pci,drive=hd0,disable-legacy=on",
        ])
    if args.arch == "x86-64":
        cmd.extend([
            "-netdev", "user,id=net0",
            "-device", "virtio-net-pci,netdev=net0,disable-legacy=on",
        ])
    for extra in args.extra_qemu_arg:
        cmd.extend(shlex.split(extra))
    return cmd


def _load_expect_file(path: str) -> list[str]:
    loaded = []
    with open(path, "r", encoding="utf-8") as fh:
        for raw in fh:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            loaded.append(line)
    return loaded


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--arch", default="x86-64")
    ap.add_argument("--iso", default="build/microkernel.iso")
    ap.add_argument("--timeout", type=float, default=120.0)
    ap.add_argument("--expect", action="append", default=[])
    ap.add_argument("--expect-file", action="append", default=[])
    ap.add_argument("--cov-out", default=None)
    ap.add_argument("--wait-cov", action="store_true")
    ap.add_argument("--extra-qemu-arg", action="append", default=[])
    ap.add_argument(
        "--blk-img",
        default=str(_ROOT / "build" / "test-blk.img"),
    )
    ap.add_argument("--cpu", default=None)
    ap.add_argument("--smp", type=int, default=2)
    ap.add_argument("--qemu", default=None)
    args = ap.parse_args()

    expects = list(args.expect)
    for ef in args.expect_file:
        expects.extend(_load_expect_file(ef))

    if not expects:
        print("test_runner: no --expect patterns given", file=sys.stderr)
        return 2

    decoder = (
        CoverageStreamDecoder(Path(args.cov_out)) if args.cov_out else None
    )
    patterns = [re.compile(p) for p in expects]
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
                if decoder is not None and decoder.feed_line(line):
                    if all(matched) and (not args.wait_cov or decoder.complete):
                        success.set()
                    continue
                sys.stdout.write(line)
                sys.stdout.flush()
                tail.append(line)
                del tail[:-200]
                for i, pat in enumerate(patterns):
                    if not matched[i] and pat.search(line):
                        matched[i] = True
                if all(matched) and (
                    not args.wait_cov
                    or (decoder is not None and decoder.complete)
                ):
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

    missing = [expects[i] for i, m in enumerate(matched) if not m]
    if decoder is not None:
        print(
            f"[test_runner] captured {len(decoder.saved_files)} .profraw "
            f"files in {args.cov_out}"
        )
        if decoder.errors:
            print(
                f"[test_runner] coverage decode errors: {decoder.errors}",
                file=sys.stderr,
            )
            return 1
    if success.is_set():
        print(f"[test_runner] PASS: all {len(patterns)} expectations matched")
        return 0
    print(f"[test_runner] {result}")
    print(f"[test_runner] missing expectations: {missing}")
    if args.wait_cov and (decoder is None or not decoder.complete):
        print("[test_runner] missing @@COV_COMPLETE@@ coverage marker")
    print("[test_runner] --- last serial output ---")
    for line in tail[-60:]:
        sys.stdout.write("    " + line)
    return 1


if __name__ == "__main__":
    sys.exit(main())
