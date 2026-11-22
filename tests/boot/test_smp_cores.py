##
# @file     test_smp_cores.py
# @brief    CTest verifying multi-core SMP boot and concurrency under -smp 4
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

# need:: VER_PROC_0007
"""Boot the ISO under QEMU -smp 4 and verify 4-core SMP online and tests."""
import argparse
from pathlib import Path
import subprocess
import sys

_ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--iso", default="build/microkernel.iso")
    ap.add_argument("--timeout", type=float, default=90.0)
    args = ap.parse_args()

    cmd = [
        sys.executable,
        str(_ROOT / "tests" / "test_runner.py"),
        "--iso", args.iso,
        "--timeout", str(args.timeout),
        "--smp", "4",
        "--blk-img", "none",
        "--expect", r"\[OK\] SMP online: 4 CPUs ready",
        "--expect", r"\[OK\] SMP SysStats reports 4 online CPUs",
        "--expect", r"\[OK\] SMP multi-core atomic ping-pong",
        "--expect", r"\[OK\] SMP concurrent futex, mutex, condvar, and sem",
        "--expect", r"\[OK\] SMP concurrent PMM, VMM, TLB shootdown, and COW",
        "--expect", r"\[OK\] SMP concurrent IPC and XSAVE/FMA isolation",
    ]
    res = subprocess.run(cmd, check=False)
    return res.returncode


if __name__ == "__main__":
    sys.exit(main())
