##
# @file     test_blk_persist.py
# @brief    Verify Ring 3 /dev/blk sector persistence across QEMU cold reboots
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Verify /dev/blk virtio-blk sector persistence across two QEMU restarts."""

# need:: VER_IPC_0008

from __future__ import annotations

import argparse
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
_ISO = ROOT / "build" / "microkernel.iso"


class BlkPersistenceTests(unittest.TestCase):
    """Cold-reboot persistence tests over a raw QEMU virtio-blk image."""

    def test_sector_persistence_across_qemu_restarts(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            img = Path(tmpdir) / "persist.img"
            img.write_bytes(b"\x00" * (2 * 1024 * 1024))

            # Boot #1: fresh zeroed disk -> writes gen=1 sentinel to LBA 0.
            cmd1 = [
                sys.executable,
                str(ROOT / "tests" / "test_runner.py"),
                "--arch",
                "x86-64",
                "--iso",
                str(_ISO),
                "--blk-img",
                str(img),
                "--timeout",
                "60",
                "--expect",
                r"\[OK\] blkdrv registered /dev/blk",
                "--expect",
                r"blktest /dev/blk sector read/write and IOAPIC IRQ pulses"
                r" verified \(gen=1 ",
            ]
            p1 = subprocess.run(cmd1, capture_output=True, text=True)
            self.assertEqual(
                p1.returncode,
                0,
                f"Boot #1 failed:\n{p1.stdout}\n{p1.stderr}",
            )

            # Confirm raw host disk image contains MKBLK_PERSIST_V1 + gen=1.
            sector0 = img.read_bytes()[:24]
            self.assertEqual(sector0[:16], b"MKBLK_PERSIST_V1")
            gen, csum = struct.unpack("<II", sector0[16:24])
            self.assertEqual(gen, 1)
            self.assertEqual(csum, 1 ^ 0xA55A1234)

            # Boot #2: cold reboot on SAME image -> must observe gen=1 on disk.
            cmd2 = [
                sys.executable,
                str(ROOT / "tests" / "test_runner.py"),
                "--arch",
                "x86-64",
                "--iso",
                str(_ISO),
                "--blk-img",
                str(img),
                "--timeout",
                "60",
                "--expect",
                r"blktest verified cross-reboot sector persistence \(gen=1\)",
                "--expect",
                r"blktest /dev/blk sector read/write and IOAPIC IRQ pulses"
                r" verified \(gen=2 ",
            ]
            p2 = subprocess.run(cmd2, capture_output=True, text=True)
            self.assertEqual(
                p2.returncode,
                0,
                f"Boot #2 failed:\n{p2.stdout}\n{p2.stderr}",
            )


def main() -> int:
    global _ISO
    ap = argparse.ArgumentParser()
    ap.add_argument("--iso", default=str(_ISO))
    args, _ = ap.parse_known_args()
    _ISO = Path(args.iso)
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(
        BlkPersistenceTests
    )
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
