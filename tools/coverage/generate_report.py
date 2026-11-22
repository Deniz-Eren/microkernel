##
# @file     generate_report.py
# @brief    Decode framed LLVM profiles and generate HTML, LCOV, and text output
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Decode base64-framed LLVM Raw Profile v8 records and run llvm-cov."""

# need:: COMP_DOCS_0004

from __future__ import annotations

import argparse
import base64
import binascii
import re
import struct
import subprocess
import sys
from pathlib import Path

LLVM_PROF_RAW_MAGIC_64 = 0xFF6C70726F667281
LLVM_PROF_RAW_HEADER_BYTES = 88

_BEGIN_RE = re.compile(r"^@@COV_BEGIN ([A-Za-z0-9_.-]+)@@$")
_END_MARKER = "@@COV_END@@"
_COMPLETE_MARKER = "@@COV_COMPLETE@@"


class CoverageStreamDecoder:
    """Parse @@COV_BEGIN / @@COV_END / @@COV_COMPLETE lines into .profraw."""

    def __init__(self, out_dir: Path) -> None:
        self.out_dir = out_dir
        self.out_dir.mkdir(parents=True, exist_ok=True)
        self._active_name: str | None = None
        self._chunks: list[str] = []
        self._index = 0
        self.saved_files: list[Path] = []
        self.errors: list[str] = []
        self.complete = False

    def feed_line(self, line: str) -> bool:
        """Consume one serial line; return True if it was a coverage line."""
        stripped = line.strip()
        if stripped == _COMPLETE_MARKER:
            self.complete = True
            return True

        m = _BEGIN_RE.match(stripped)
        if m is not None:
            if self._active_name is not None:
                self.errors.append(
                    f"interrupted coverage frame '{self._active_name}'"
                )
            self._active_name = m.group(1)
            self._chunks = []
            return True

        if self._active_name is not None:
            if stripped == _END_MARKER:
                self._flush_frame(self._active_name, "".join(self._chunks))
                self._active_name = None
                self._chunks = []
                return True
            self._chunks.append(stripped)
            return True

        return False

    def _flush_frame(self, name: str, payload: str) -> None:
        try:
            raw = base64.b64decode(payload, validate=True)
        except (binascii.Error, ValueError) as exc:
            self.errors.append(f"invalid base64 in frame '{name}': {exc}")
            return

        if len(raw) < LLVM_PROF_RAW_HEADER_BYTES:
            self.errors.append(
                f"truncated profraw header in frame '{name}' "
                f"({len(raw)} bytes)"
            )
            return

        magic = struct.unpack_from("<Q", raw, 0)[0]
        if magic != LLVM_PROF_RAW_MAGIC_64:
            self.errors.append(
                f"unexpected profraw magic 0x{magic:016x} in frame '{name}'"
            )
            return

        safe_name = re.sub(r"[^A-Za-z0-9_.-]+", "_", name) or "unknown"
        dest = self.out_dir / f"{self._index:04d}_{safe_name}.profraw"
        self._index += 1
        dest.write_bytes(raw)
        self.saved_files.append(dest)


def elf_has_covmap(path: Path) -> bool:
    """Return True when path is an ELF64 file with a __llvm_covmap section."""
    try:
        data = path.read_bytes()
    except OSError:
        return False
    if len(data) < 64 or data[:4] != b"\x7fELF":
        return False
    if data[4] != 2 or data[5] != 1:  # ELF64 little-endian
        return False
    e_shoff = struct.unpack_from("<Q", data, 40)[0]
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", data, 58)
    if (
        e_shoff == 0
        or e_shentsize < 64
        or e_shnum == 0
        or e_shstrndx >= e_shnum
    ):
        return False
    shstr_hdr = e_shoff + e_shstrndx * e_shentsize
    if shstr_hdr + 40 > len(data):
        return False
    shstr_off, shstr_sz = struct.unpack_from("<QQ", data, shstr_hdr + 24)
    if shstr_off + shstr_sz > len(data):
        return False
    shstr = data[shstr_off : shstr_off + shstr_sz]

    for idx in range(e_shnum):
        hdr = e_shoff + idx * e_shentsize
        if hdr + 4 > len(data):
            break
        name_off = struct.unpack_from("<I", data, hdr)[0]
        if name_off >= len(shstr):
            continue
        end = shstr.find(b"\x00", name_off)
        sec_name = shstr[name_off : end if end != -1 else len(shstr)]
        if sec_name == b"__llvm_covmap":
            return True
    return False


def discover_cov_objects(build_dir: Path) -> list[Path]:
    """Find kernel.elf and Ring 3 ELF binaries carrying __llvm_covmap."""
    found: list[Path] = []
    kernel_elf = build_dir / "kernel.elf"
    if kernel_elf.is_file() and elf_has_covmap(kernel_elf):
        found.append(kernel_elf)

    skip_suffixes = {".o", ".a", ".d", ".cmake", ".ninja", ".txt", ".json"}
    for subdir in ("servers", "bin", "tests"):
        root = build_dir / subdir
        if not root.is_dir():
            continue
        for candidate in sorted(root.rglob("*")):
            if not candidate.is_file():
                continue
            if "CMakeFiles" in candidate.parts:
                continue
            if candidate.suffix in skip_suffixes:
                continue
            if elf_has_covmap(candidate):
                found.append(candidate)
    return found


def build_llvm_commands(
    profraws: list[Path],
    objects: list[Path],
    out_dir: Path,
    profdata_bin: str = "llvm-profdata",
    cov_bin: str = "llvm-cov",
) -> dict[str, list[str]]:
    """Construct llvm-profdata merge and llvm-cov report/show/export cmds."""
    profdata = out_dir / "merged.profdata"
    html_dir = out_dir / "html"

    merge_cmd = [
        profdata_bin,
        "merge",
        "-sparse",
        *(str(p) for p in profraws),
        "-o",
        str(profdata),
    ]

    obj_args = [str(objects[0])]
    for obj in objects[1:]:
        obj_args.extend(["-object", str(obj)])
    common_cov = [
        *obj_args,
        f"-instr-profile={profdata}",
    ]

    report_cmd = [cov_bin, "report", *common_cov, "--show-branch-summary"]
    html_cmd = [
        cov_bin,
        "show",
        *common_cov,
        "-format=html",
        f"-output-dir={html_dir}",
        "-show-line-counts-or-regions",
        "-show-branches=count",
    ]
    lcov_cmd = [cov_bin, "export", *common_cov, "-format=lcov"]
    return {
        "merge": merge_cmd,
        "report": report_cmd,
        "html": html_cmd,
        "lcov": lcov_cmd,
    }


def generate_reports(
    raw_dir: Path,
    build_dir: Path,
    out_dir: Path,
    profdata_bin: str = "llvm-profdata",
    cov_bin: str = "llvm-cov",
) -> int:
    """Merge raw profiles and emit summary.txt, html/index.html, lcov.info."""
    profraws = sorted(raw_dir.glob("*.profraw"))
    if not profraws:
        print(
            f"[coverage] error: no .profraw files found in {raw_dir}",
            file=sys.stderr,
        )
        return 1

    objects = discover_cov_objects(build_dir)
    if not objects:
        print(
            f"[coverage] error: no instrumented ELF objects in {build_dir}",
            file=sys.stderr,
        )
        return 1

    out_dir.mkdir(parents=True, exist_ok=True)
    cmds = build_llvm_commands(
        profraws, objects, out_dir, profdata_bin=profdata_bin, cov_bin=cov_bin
    )

    print(
        f"[coverage] merging {len(profraws)} profiles across "
        f"{len(objects)} ELF objects"
    )
    subprocess.run(cmds["merge"], check=True)

    rep = subprocess.run(
        cmds["report"], check=True, capture_output=True, text=True
    )
    summary_path = out_dir / "summary.txt"
    summary_path.write_text(rep.stdout, encoding="utf-8")
    sys.stdout.write(rep.stdout)

    subprocess.run(cmds["html"], check=True)

    lcov = subprocess.run(
        cmds["lcov"], check=True, capture_output=True, text=True
    )
    lcov_path = out_dir / "lcov.info"
    lcov_path.write_text(lcov.stdout, encoding="utf-8")

    print(f"[coverage] HTML report : {out_dir / 'html' / 'index.html'}")
    print(f"[coverage] LCOV report : {lcov_path}")
    print(f"[coverage] Text summary: {summary_path}")
    return 0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        description="Merge LLVM .profraw profiles and generate coverage output."
    )
    ap.add_argument("--raw-dir", required=True, type=Path)
    ap.add_argument("--build-dir", required=True, type=Path)
    ap.add_argument("--out-dir", required=True, type=Path)
    ap.add_argument("--profdata-bin", default="llvm-profdata")
    ap.add_argument("--cov-bin", default="llvm-cov")
    args = ap.parse_args(argv)
    return generate_reports(
        raw_dir=args.raw_dir,
        build_dir=args.build_dir,
        out_dir=args.out_dir,
        profdata_bin=args.profdata_bin,
        cov_bin=args.cov_bin,
    )


if __name__ == "__main__":
    raise SystemExit(main())
