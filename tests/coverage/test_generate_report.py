##
# @file     test_generate_report.py
# @brief    Unit tests for framed LLVM profile decoding and report commands
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Unit tests for tools/coverage/generate_report.py."""

# need:: VER_DOCS_0006

from __future__ import annotations

import base64
import struct
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "coverage"))

from generate_report import (  # noqa: E402
    LLVM_PROF_RAW_MAGIC_64,
    CoverageStreamDecoder,
    build_llvm_commands,
    discover_cov_objects,
    elf_has_covmap,
)


def _make_fake_profraw(payload: bytes = b"test-data") -> bytes:
    hdr = struct.pack(
        "<11Q",
        LLVM_PROF_RAW_MAGIC_64,
        8,
        0,
        1,
        0,
        1,
        0,
        len(payload),
        0,
        0,
        1,
    )
    return hdr + payload


def _make_fake_elf64(sections: list[bytes]) -> bytes:
    """Build a minimal ELF64 little-endian image with named section headers."""
    shstr = bytearray(b"\x00")
    offsets: list[int] = []
    for name in sections:
        offsets.append(len(shstr))
        shstr.extend(name + b"\x00")
    shstr_name_off = len(shstr)
    shstr.extend(b".shstrtab\x00")

    ehdr_size = 64
    shstr_off = ehdr_size
    sh_off = shstr_off + len(shstr)
    sh_num = len(sections) + 2  # null + user sections + .shstrtab
    shstr_idx = len(sections) + 1

    ehdr = bytearray(ehdr_size)
    ehdr[0:4] = b"\x7fELF"
    ehdr[4] = 2  # ELFCLASS64
    ehdr[5] = 1  # ELFDATA2LSB
    struct.pack_into("<Q", ehdr, 40, sh_off)
    struct.pack_into("<HHH", ehdr, 58, 64, sh_num, shstr_idx)

    shdrs = bytearray(64)  # SHT_NULL
    for off in offsets:
        sh = bytearray(64)
        struct.pack_into("<I", sh, 0, off)
        shdrs.extend(sh)
    shstr_hdr = bytearray(64)
    struct.pack_into("<I", shstr_hdr, 0, shstr_name_off)
    struct.pack_into("<QQ", shstr_hdr, 24, shstr_off, len(shstr))
    shdrs.extend(shstr_hdr)
    return bytes(ehdr) + bytes(shstr) + bytes(shdrs)


class CoveragePipelineTests(unittest.TestCase):
    """Verify base64 profile framing, ELF detection, and llvm-cov commands."""

    def test_decode_multiple_frames_and_passthrough(self) -> None:
        raw1 = _make_fake_profraw(b"alpha")
        raw2 = _make_fake_profraw(b"beta-longer")
        b64_1 = base64.b64encode(raw1).decode("ascii")
        b64_2 = base64.b64encode(raw2).decode("ascii")

        with tempfile.TemporaryDirectory() as tmp:
            dec = CoverageStreamDecoder(Path(tmp))
            self.assertFalse(dec.feed_line("[OK] boot started\n"))
            self.assertTrue(dec.feed_line("@@COV_BEGIN sh@@\n"))
            self.assertTrue(dec.feed_line(b64_1[:40] + "\n"))
            self.assertTrue(dec.feed_line(b64_1[40:] + "\n"))
            self.assertTrue(dec.feed_line("@@COV_END@@\n"))
            self.assertFalse(dec.feed_line("[OK] second line\n"))
            self.assertTrue(dec.feed_line("@@COV_BEGIN sh@@\n"))
            self.assertTrue(dec.feed_line(b64_2 + "\n"))
            self.assertTrue(dec.feed_line("@@COV_END@@\n"))
            self.assertFalse(dec.complete)
            self.assertTrue(dec.feed_line("@@COV_COMPLETE@@\n"))
            self.assertTrue(dec.complete)
            self.assertEqual(dec.errors, [])
            self.assertEqual(len(dec.saved_files), 2)
            self.assertEqual(dec.saved_files[0].name, "0000_sh.profraw")
            self.assertEqual(dec.saved_files[1].name, "0001_sh.profraw")
            self.assertEqual(dec.saved_files[0].read_bytes(), raw1)
            self.assertEqual(dec.saved_files[1].read_bytes(), raw2)

    def test_rejects_bad_base64_and_bad_magic(self) -> None:
        bad_magic = struct.pack(
            "<11Q", 0x12345678, 8, 0, 1, 0, 1, 0, 0, 0, 0, 1
        )
        b64_bad_magic = base64.b64encode(bad_magic).decode("ascii")

        with tempfile.TemporaryDirectory() as tmp:
            dec = CoverageStreamDecoder(Path(tmp))
            dec.feed_line("@@COV_BEGIN broken@@\n")
            dec.feed_line("!!!not-base64!!!\n")
            dec.feed_line("@@COV_END@@\n")
            dec.feed_line("@@COV_BEGIN wrongmagic@@\n")
            dec.feed_line(b64_bad_magic + "\n")
            dec.feed_line("@@COV_END@@\n")
            self.assertEqual(dec.saved_files, [])
            self.assertEqual(len(dec.errors), 2)

    def test_elf_covmap_and_object_discovery(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            bdir = Path(tmp)
            (bdir / "servers" / "sysproc").mkdir(parents=True)
            (bdir / "servers" / "sysproc" / "CMakeFiles").mkdir(parents=True)
            (bdir / "bin").mkdir(parents=True)

            k = bdir / "kernel.elf"
            k.write_bytes(_make_fake_elf64([b".text", b"__llvm_covmap"]))
            s = bdir / "servers" / "sysproc" / "sysproc"
            s.write_bytes(_make_fake_elf64([b".text", b"__llvm_covmap"]))
            o = bdir / "servers" / "sysproc" / "CMakeFiles" / "main.o"
            o.write_bytes(_make_fake_elf64([b".text", b"__llvm_covmap"]))
            plain = bdir / "bin" / "plain"
            plain.write_bytes(_make_fake_elf64([b".text", b".data"]))

            self.assertTrue(elf_has_covmap(k))
            self.assertFalse(elf_has_covmap(plain))
            found = discover_cov_objects(bdir)
            self.assertEqual(found, [k, s])

    def test_build_llvm_commands(self) -> None:
        out = Path("/tmp/cov-out")
        cmds = build_llvm_commands(
            profraws=[Path("/tmp/raw/0000_kernel.profraw")],
            objects=[Path("/tmp/build/kernel.elf"), Path("/tmp/build/sh")],
            out_dir=out,
        )
        self.assertEqual(
            cmds["merge"][:3], ["llvm-profdata", "merge", "-sparse"]
        )
        self.assertIn("-object", cmds["report"])
        self.assertNotIn("-ignore-filename-regex=(^|/)tests/", cmds["report"])
        self.assertIn("-format=html", cmds["html"])
        self.assertIn("-show-branches=count", cmds["html"])
        self.assertIn("-format=lcov", cmds["lcov"])


if __name__ == "__main__":
    ok = unittest.main(exit=False).result.wasSuccessful()
    raise SystemExit(0 if ok else 1)
