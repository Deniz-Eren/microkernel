##
# @file     test_tzcode.py
# @brief    CTest check for sysroot IANA tzcode ELF linkage and packaging
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Verify IANA tzcode dynamic linkage, tzselect generation, and boot.tar."""

# need:: VER_LIBC_0039

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


class TzcodePackagingTests(unittest.TestCase):
    """Verify sysroot tzcode binaries, tzselect script, and boot.tar entries."""

    def test_dynamic_linkage_and_sysroot(self) -> None:
        zic = _SYSROOT / "usr" / "sbin" / "zic"
        zdump = _SYSROOT / "usr" / "bin" / "zdump"
        tzselect = _SYSROOT / "usr" / "bin" / "tzselect"
        for path in (zic, zdump, tzselect):
            self.assertTrue(path.is_file(), f"missing sysroot file {path}")

        for binary in (zic, zdump):
            out = subprocess.check_output(
                [_READELF, "-h", "-l", "-d", str(binary)],
                text=True,
            )
            self.assertIn("Elf file type is DYN", out)
            self.assertIn("PIE", out)
            self.assertIn("/boot/ld.so", out)
            self.assertIn("Shared library: [libc.so]", out)

        script = tzselect.read_text(encoding="utf-8")
        self.assertTrue(script.startswith("#!/bin/sh\n"))
        self.assertIn("TZDIR='/usr/share/zoneinfo'", script)
        self.assertIn("TZVERSION='2025b'", script)

        for name in (
            "Australia/Sydney",
            "America/New_York",
            "Europe/London",
            "UTC",
        ):
            zp = _SYSROOT / "usr" / "share" / "zoneinfo" / name
            self.assertTrue(zp.is_file(), f"missing sysroot TZif {name}")
            self.assertEqual(zp.read_bytes()[:4], b"TZif")
        for name in ("iso3166.tab", "zone1970.tab"):
            self.assertTrue(
                (_SYSROOT / "usr" / "share" / "zoneinfo" / name).is_file(),
                f"missing sysroot zoneinfo table {name}",
            )

    def test_boot_tar_entries(self) -> None:
        self.assertTrue(_BOOT_TAR.is_file(), f"missing {_BOOT_TAR}")
        with tarfile.open(_BOOT_TAR, "r") as tf:
            members = {m.name for m in tf.getmembers() if m.isfile()}
        expected = {
            "zic",
            "zdump",
            "tzselect",
            "etcetera",
            "australasia",
            "usr/share/zoneinfo/Australia/Sydney",
            "usr/share/zoneinfo/America/New_York",
            "usr/share/zoneinfo/Europe/London",
            "usr/share/zoneinfo/UTC",
            "usr/share/zoneinfo/iso3166.tab",
            "usr/share/zoneinfo/zone1970.tab",
        }
        missing = sorted(expected - members)
        self.assertEqual(missing, [], f"missing from boot.tar: {missing}")


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
        TzcodePackagingTests
    )
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
