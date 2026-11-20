#!/usr/bin/env python3
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
"""Pack Ring 3 ELF servers and applications into a POSIX ustar boot archive.

Usage:
    mk_boot_tar.py --output build/boot.tar sysproc=build/sysproc/sysproc ...

The kernel locates entries by exact file name.
"""
import argparse
import sys


def ustar_header(name: str, size: int, mode: str = "0000644") -> bytes:
    if len(name) > 100:
        raise ValueError(f"tar name too long: {name}")
    h = bytearray(512)

    def put(off: int, data: bytes) -> None:
        h[off:off + len(data)] = data

    put(0, name.encode().ljust(100, b"\0"))            # name
    put(100, mode.encode().ljust(8, b"\0"))            # mode
    put(108, b"0000000\0")                             # uid
    put(116, b"0000000\0")                             # gid
    put(124, ("%011o" % size).encode() + b"\0")        # size
    put(136, b"00000000000\0")                         # mtime
    put(148, b"        ")                              # checksum placeholder
    put(156, b"0")                                     # typeflag: regular file
    put(257, b"ustar\0")                               # magic
    put(263, b"00")                                    # version
    put(265, b"root".ljust(32, b"\0"))                 # uname
    put(297, b"root".ljust(32, b"\0"))                 # gname
    chksum = sum(h)
    put(148, ("%06o\0 " % chksum).encode())
    return bytes(h)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--output", required=True)
    ap.add_argument("entries", nargs="*", help="name=path pairs")
    args = ap.parse_args()

    out = bytearray()
    for entry in args.entries:
        name, _, path = entry.partition("=")
        try:
            with open(path, "rb") as f:
                data = f.read()
        except OSError as e:
            print(f"mk_boot_tar: cannot read {path}: {e}", file=sys.stderr)
            return 1
        out += ustar_header(name, len(data))
        out += data
        if len(data) % 512:
            out += bytes(512 - len(data) % 512)

    out += bytes(1024)  # end-of-archive marker
    with open(args.output, "wb") as f:
        f.write(out)
    print(f"mk_boot_tar: wrote {args.output} ({len(out)} bytes, {len(args.entries)} entries)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
