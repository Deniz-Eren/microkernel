##
# @file     test_x86_64_v3.py
# @brief    Verify legacy CPU boot rejection, scalar Ring 0, and AVX2 Ring 3
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Verify x86-64-v3 negative boot gate and Ring 0/Ring 3 instruction sets."""

# need:: VER_BOOT_0004

from __future__ import annotations

import argparse
from pathlib import Path
import re
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
_ISO = ROOT / "build" / "microkernel.iso"
_KERNEL = ROOT / "build" / "kernel.elf"
_LIBC = ROOT / "sysroot" / "usr" / "lib" / "libc.a"
_SIMDTEST = ROOT / "build" / "tests" / "simdtest" / "simdtest"
_OBJDUMP = "llvm-objdump"


class X8664V3BaselineTests(unittest.TestCase):
    """Negative legacy-CPU boot gate and binary instruction-set audits."""

    def test_legacy_cpu_rejected_cleanly_on_boot(self) -> None:
        cmd = [
            sys.executable,
            str(ROOT / "tests" / "test_runner.py"),
            "--arch",
            "x86-64",
            "--iso",
            str(_ISO),
            "--cpu",
            "qemu64,+smap,+smep",
            "--timeout",
            "30",
            "--expect",
            r"\[FAIL\] unsupported CPU: x86-64-v3 baseline required",
            "--expect",
            r"\[FAIL\] system halted",
        ]
        proc = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(
            proc.returncode,
            0,
            f"legacy CPU boot gate failed:\n{proc.stdout}\n{proc.stderr}",
        )
        self.assertNotIn("UD invalid opcode", proc.stdout)
        self.assertNotIn("[OK] PMM/VMM/APIC initialized", proc.stdout)

    def test_ring0_kernel_remains_strictly_scalar(self) -> None:
        proc = subprocess.run(
            [_OBJDUMP, "-d", str(_KERNEL)],
            check=True,
            capture_output=True,
            text=True,
        )
        vec_reg = re.compile(r"%(xmm|ymm|zmm)[0-9]+\b")
        offenders = [
            line.strip()
            for line in proc.stdout.splitlines()
            if vec_reg.search(line)
        ]
        self.assertEqual(
            offenders,
            [],
            f"Ring 0 kernel emitted vector instructions: {offenders[:10]}",
        )

    def test_ring3_emits_x86_64_v3_vector_and_fma_instructions(self) -> None:
        libc_dump = subprocess.run(
            [_OBJDUMP, "-d", str(_LIBC)],
            check=True,
            capture_output=True,
            text=True,
        ).stdout
        self.assertRegex(
            libc_dump,
            r"\bvmovdqu\b.*%ymm",
            "libc.a did not emit AVX2 vmovdqu %ymm instructions",
        )
        simd_dump = subprocess.run(
            [_OBJDUMP, "-d", str(_SIMDTEST)],
            check=True,
            capture_output=True,
            text=True,
        ).stdout
        self.assertRegex(
            simd_dump,
            r"\bvfmadd",
            "simdtest did not emit hardware FMA instructions",
        )


def main() -> int:
    global _ISO, _KERNEL, _LIBC, _SIMDTEST, _OBJDUMP
    ap = argparse.ArgumentParser()
    ap.add_argument("--iso", default=str(_ISO))
    ap.add_argument("--kernel", default=str(_KERNEL))
    ap.add_argument("--libc", default=str(_LIBC))
    ap.add_argument("--simdtest", default=str(_SIMDTEST))
    ap.add_argument("--objdump", default=_OBJDUMP)
    args, rest = ap.parse_known_args()
    _ISO = Path(args.iso)
    _KERNEL = Path(args.kernel)
    _LIBC = Path(args.libc)
    _SIMDTEST = Path(args.simdtest)
    _OBJDUMP = args.objdump
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(
        X8664V3BaselineTests
    )
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
