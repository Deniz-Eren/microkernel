##
# @file     test_ext2_persist.py
# @brief    Verify Ring 3 /mnt ext2 file and dir persistence across QEMU reboots
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Verify /mnt ext2 superblock and cross-reboot file/directory persistence."""

# need:: VER_IPC_0009

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


class Ext2PersistenceTests(unittest.TestCase):
    """Cold-reboot persistence tests for vfsserv ext2 mounted on /mnt."""

    def test_ext2_files_and_dirs_persist_across_qemu_restarts(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            img = Path(tmpdir) / "ext2.img"
            img.write_bytes(b"\x00" * (2 * 1024 * 1024))

            # Boot #1: virgin disk -> vfsserv formats ext2, ext2test writes
            # /mnt/persist/counter.txt (gen=1) and 16 KiB /mnt/persist/big.bin.
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
                r"\[OK\] vfsserv formatted and mounted ext2 on /mnt",
                "--expect",
                r"ext2test /mnt create, multi-block write/read, truncate,"
                r" rename, and unlink verified \(gen=1\)",
            ]
            p1 = subprocess.run(cmd1, capture_output=True, text=True)
            self.assertEqual(
                p1.returncode,
                0,
                f"Boot #1 failed:\n{p1.stdout}\n{p1.stderr}",
            )

            # Audit raw on-disk rev-1 ext2 superblock at byte offset 1024.
            raw = img.read_bytes()
            sb = raw[1024 : 1024 + 128]
            s_inodes, s_blocks = struct.unpack_from("<II", sb, 0)
            s_log_bsize = struct.unpack_from("<I", sb, 24)[0]
            s_magic = struct.unpack_from("<H", sb, 56)[0]
            s_rev = struct.unpack_from("<I", sb, 76)[0]
            s_inode_sz = struct.unpack_from("<H", sb, 88)[0]
            self.assertEqual(s_magic, 0xEF53)
            self.assertEqual(s_log_bsize, 0)
            self.assertEqual(s_rev, 1)
            self.assertEqual(s_inode_sz, 128)
            self.assertEqual(s_inodes, 256)
            self.assertGreater(s_blocks, 100)

            # Boot #2: cold reboot on SAME disk -> mounts existing ext2 and
            # verifies /mnt/persist/counter.txt (gen=1) + 16 KiB big.bin.
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
                r"\[OK\] vfsserv mounted existing ext2 on /mnt",
                "--expect",
                r"ext2test verified cross-reboot /mnt file and dir persistence"
                r" \(gen=1\)",
                "--expect",
                r"ext2test /mnt create, multi-block write/read, truncate,"
                r" rename, and unlink verified \(gen=2\)",
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
        Ext2PersistenceTests
    )
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
