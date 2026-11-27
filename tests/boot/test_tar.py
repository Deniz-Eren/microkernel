##
# @file     test_tar.py
# @brief    CTest check for sysroot libarchive bsdtar ELF linkage and packaging
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Verify libarchive bsdtar dynamic linkage, sysroot install, and boot.tar."""

# need:: VER_LIBC_0040

from __future__ import annotations

import argparse
import subprocess
import sys
import tarfile
import unittest
from pathlib import Path

_SYSROOT = Path("sysroot")
_BOOT_TAR = Path("build/boot.tar")
_READELF = "llvm-readelf"


class TarPackagingTests(unittest.TestCase):
    """Verify the sysroot tar binary and its boot.tar entry."""

    def test_dynamic_linkage_and_sysroot(self) -> None:
        tar_bin = _SYSROOT / "usr" / "bin" / "tar"
        self.assertTrue(tar_bin.is_file(), f"missing sysroot file {tar_bin}")

        out = subprocess.check_output(
            [_READELF, "-h", "-l", "-d", str(tar_bin)],
            text=True,
        )
        self.assertIn("Elf file type is DYN", out)
        self.assertIn("PIE", out)
        self.assertIn("/boot/ld.so", out)
        self.assertIn("Shared library: [libc.so]", out)
        self.assertIn("Shared library: [libz.so.1]", out)

    def test_boot_tar_entries(self) -> None:
        self.assertTrue(_BOOT_TAR.is_file(), f"missing {_BOOT_TAR}")
        with tarfile.open(_BOOT_TAR, "r") as tf:
            members = {m.name for m in tf.getmembers() if m.isfile()}
        self.assertIn("tar", members, "missing tar from boot.tar")


def main() -> int:
    global _SYSROOT, _BOOT_TAR, _READELF
    ap = argparse.ArgumentParser()
    ap.add_argument("--sysroot", default=str(_SYSROOT))
    ap.add_argument("--boot-tar", default=str(_BOOT_TAR))
    ap.add_argument("--readelf", default=_READELF)
    args, _ = ap.parse_known_args()
    _SYSROOT = Path(args.sysroot)
    _BOOT_TAR = Path(args.boot_tar)
    _READELF = args.readelf
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(
        TarPackagingTests
    )
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
