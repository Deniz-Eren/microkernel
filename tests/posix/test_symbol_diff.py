##
# @file     test_symbol_diff.py
# @brief    Gate 2 AST and libc.a symbol diff against POSIX.1-2024 manifest
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Gate 2: AST and archive symbol diff against `issue8-manifest.tsv`."""

# need:: VER_DOCS_0004

from __future__ import annotations

import argparse
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "posix"))

from audit_lib import (  # noqa: E402
    ALT_OPTION_GROUPS,
    BASE_TARGET_GROUPS,
    OPTION_GROUP_MACROS,
    PosixManifest,
    detect_clang_include,
    extract_archive_symbols,
    extract_bin_utilities,
    extract_defined_macros,
    extract_header_ast_symbols,
    extract_headers_defined_macros,
    extract_tar_members,
    is_reserved_identifier,
    load_manifest,
)

DEFAULT_SYSROOT = ROOT / "sysroot" / "usr" / "include"
DEFAULT_LIBC = ROOT / "sysroot" / "usr" / "lib" / "libc.a"
DEFAULT_MANIFEST = ROOT / "tests" / "posix" / "issue8-manifest.tsv"
DEFAULT_BIN_CMAKE = ROOT / "bin" / "CMakeLists.txt"

SH_BUILTIN_UTILITIES: tuple[str, ...] = (
    ".",
    ":",
    "alias",
    "cd",
    "command",
    "getopts",
    "hash",
    "pwd",
    "read",
    "ulimit",
    "umask",
    "unalias",
    "wait",
)

IMPLEMENTED_POSIX_HEADERS: tuple[str, ...] = (
    "aio.h",
    "arpa/inet.h",
    "assert.h",
    "complex.h",
    "cpio.h",
    "ctype.h",
    "dirent.h",
    "dlfcn.h",
    "endian.h",
    "errno.h",
    "fcntl.h",
    "fenv.h",
    "float.h",
    "fnmatch.h",
    "glob.h",
    "grp.h",
    "iconv.h",
    "inttypes.h",
    "iso646.h",
    "langinfo.h",
    "libintl.h",
    "limits.h",
    "locale.h",
    "math.h",
    "monetary.h",
    "net/if.h",
    "netdb.h",
    "netinet/in.h",
    "netinet/tcp.h",
    "nl_types.h",
    "poll.h",
    "pthread.h",
    "pwd.h",
    "regex.h",
    "sched.h",
    "semaphore.h",
    "setjmp.h",
    "signal.h",
    "stdalign.h",
    "stdarg.h",
    "stdatomic.h",
    "stdbool.h",
    "stddef.h",
    "stdint.h",
    "stdio.h",
    "stdlib.h",
    "stdnoreturn.h",
    "string.h",
    "strings.h",
    "tar.h",
    "termios.h",
    "tgmath.h",
    "threads.h",
    "time.h",
    "uchar.h",
    "unistd.h",
    "wchar.h",
    "wctype.h",
    "wordexp.h",
    "sys/mman.h",
    "sys/resource.h",
    "sys/select.h",
    "sys/socket.h",
    "sys/stat.h",
    "sys/statvfs.h",
    "sys/times.h",
    "sys/types.h",
    "sys/un.h",
    "sys/utsname.h",
    "sys/wait.h",
)

_COMPILER = "clang"
_TARGET = "x86_64-unknown-none-elf"
_CLANG_INC: Path | None = None
_SYSROOT = DEFAULT_SYSROOT
_NM = "llvm-nm"
_LIBC = DEFAULT_LIBC
_MANIFEST = DEFAULT_MANIFEST
_BIN_CMAKE = DEFAULT_BIN_CMAKE
_BOOT_TAR: Path | None = None
_REPORT: Path | None = None


def _is_option_allowed(sym: str, group: str, active_macros: set[str]) -> bool:
    if group in BASE_TARGET_GROUPS:
        return True
    groups = (group, *ALT_OPTION_GROUPS.get(sym, ()))
    for grp in groups:
        macros = OPTION_GROUP_MACROS.get(grp, ())
        if any(m in active_macros for m in macros):
            return True
    return False


def _classify_symbols(
    symbols: set[str],
    manifest: PosixManifest,
    active_macros: set[str],
) -> tuple[list[str], list[tuple[str, str]], list[str]]:
    """Return `(hygiene_violations, ungated_options, reserved_non_manifest)`."""
    violations: list[str] = []
    ungated: list[tuple[str, str]] = []
    reserved: list[str] = []
    for sym in sorted(symbols):
        entry = manifest.interfaces.get(sym)
        if entry is not None:
            if not _is_option_allowed(sym, entry.group, active_macros):
                ungated.append((sym, entry.group))
        elif is_reserved_identifier(sym):
            reserved.append(sym)
        else:
            violations.append(sym)
    return violations, ungated, reserved


def _write_diff_report(
    report_path: Path,
    manifest: PosixManifest,
    ast_symbols: set[str],
    archive_symbols: set[str],
    header_macros: set[str],
    active_macros: set[str],
    bin_utilities: set[str],
) -> None:
    base_hdrs = manifest.base_headers()
    impl_hdrs = set(IMPLEMENTED_POSIX_HEADERS) & base_hdrs
    missing_hdrs = sorted(base_hdrs - impl_hdrs)

    base_ifaces = manifest.base_interfaces()
    macro_ifaces = (header_macros & base_ifaces) - archive_symbols
    impl_ifaces = (archive_symbols | macro_ifaces) & base_ifaces
    missing_ifaces = sorted(base_ifaces - impl_ifaces)

    base_utils = manifest.base_utilities()
    builtin_utils = set(SH_BUILTIN_UTILITIES) & base_utils
    impl_utils = (bin_utilities | builtin_utils) & base_utils
    missing_utils = sorted(base_utils - impl_utils)

    active_opt_groups = sorted(
        grp
        for grp, macros in OPTION_GROUP_MACROS.items()
        if any(m in active_macros for m in macros)
    )
    reserved_exports = sorted(
        s
        for s in archive_symbols
        if s not in manifest.interfaces and is_reserved_identifier(s)
    )

    lines = [
        "# POSIX.1-2024 (Issue 8) Symbol Diff Report (Work-in-Progress Target)",
        f"headers_base_implemented: {len(impl_hdrs)}/{len(base_hdrs)}",
        f"interfaces_base_implemented: {len(impl_ifaces)}/{len(base_ifaces)}",
        f"utilities_base_implemented: {len(impl_utils)}/{len(base_utils)}",
        f"header_ast_declarations: {len(ast_symbols)}",
        f"header_macro_interfaces: {len(macro_ifaces)}",
        f"libc_exported_symbols: {len(archive_symbols)}",
        f"bin_utilities_built: {len(bin_utilities)}",
        f"sh_builtin_utilities: {len(builtin_utils)}",
        f"active_option_groups: {', '.join(active_opt_groups) or 'none'}",
        f"reserved_internal_exports: {', '.join(reserved_exports) or 'none'}",
        "",
        "## Missing BASE/CX/OB Headers",
        *missing_hdrs,
        "",
        "## Missing BASE/CX/OB Interfaces",
        *missing_ifaces,
        "",
        "## Missing BASE/CX/OB Utilities",
        *missing_utils,
        "",
    ]
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text("\n".join(lines), encoding="utf-8")


class PosixSymbolDiffTests(unittest.TestCase):
    """Gate 2: AST and archive symbol diff against `issue8-manifest.tsv`."""

    def test_ast_and_libc_symbol_hygiene_and_diff(self) -> None:
        self.assertTrue(_MANIFEST.is_file(), f"missing {_MANIFEST}")
        self.assertTrue(_LIBC.is_file(), f"missing {_LIBC}")

        manifest = load_manifest(_MANIFEST)
        self.assertGreater(len(manifest.base_headers()), 50)
        self.assertGreater(len(manifest.base_interfaces()), 900)
        self.assertGreater(len(manifest.base_utilities()), 90)

        self.assertTrue(_BIN_CMAKE.is_file(), f"missing {_BIN_CMAKE}")
        bin_utilities = extract_bin_utilities(_BIN_CMAKE)
        base_utilities = manifest.base_utilities()
        non_posix_bins = sorted(bin_utilities - base_utilities)
        self.assertEqual(
            non_posix_bins,
            [],
            f"non-POSIX or non-Base binaries in bin/: "
            f"{', '.join(non_posix_bins)}",
        )
        non_posix_builtins = sorted(set(SH_BUILTIN_UTILITIES) - base_utilities)
        self.assertEqual(
            non_posix_builtins,
            [],
            f"non-POSIX sh built-ins listed: {', '.join(non_posix_builtins)}",
        )
        if _BOOT_TAR is not None:
            self.assertTrue(_BOOT_TAR.is_file(), f"missing {_BOOT_TAR}")
            tar_members = extract_tar_members(_BOOT_TAR)
            missing_in_tar = sorted(bin_utilities - tar_members)
            self.assertEqual(
                missing_in_tar,
                [],
                f"bin/ utilities missing from boot.tar: "
                f"{', '.join(missing_in_tar)}",
            )

        clang_inc = _CLANG_INC or detect_clang_include(_COMPILER)
        active_macros = extract_defined_macros(
            _COMPILER, clang_inc, _SYSROOT, "unistd.h", target=_TARGET
        )
        self.assertIn("_POSIX_VERSION", active_macros)

        ast_symbols = extract_header_ast_symbols(
            _COMPILER,
            clang_inc,
            _SYSROOT,
            IMPLEMENTED_POSIX_HEADERS,
            target=_TARGET,
        )
        archive_symbols = extract_archive_symbols(_NM, _LIBC)

        ast_bad, ast_opt, _ = _classify_symbols(
            ast_symbols, manifest, active_macros
        )
        self.assertEqual(
            ast_bad,
            [],
            f"non-standard header AST declarations: {', '.join(ast_bad)}",
        )
        self.assertEqual(
            ast_opt,
            [],
            f"ungated option-group header declarations: {ast_opt}",
        )

        arc_bad, arc_opt, _ = _classify_symbols(
            archive_symbols, manifest, active_macros
        )
        self.assertEqual(
            arc_bad,
            [],
            f"non-standard libc.a exported symbols: {', '.join(arc_bad)}",
        )
        self.assertEqual(
            arc_opt,
            [],
            f"ungated option-group libc.a exports: {arc_opt}",
        )

        declared_posix = {
            s for s in ast_symbols if s in manifest.interfaces
        }
        missing_defs = sorted(declared_posix - archive_symbols)
        self.assertEqual(
            missing_defs,
            [],
            f"declared POSIX symbols missing from libc.a: "
            f"{', '.join(missing_defs)}",
        )

        if _REPORT is not None:
            header_macros = extract_headers_defined_macros(
                _COMPILER,
                clang_inc,
                _SYSROOT,
                IMPLEMENTED_POSIX_HEADERS,
                target=_TARGET,
            )
            _write_diff_report(
                _REPORT,
                manifest,
                ast_symbols,
                archive_symbols,
                header_macros,
                active_macros,
                bin_utilities,
            )
            self.assertTrue(_REPORT.is_file())


def main(argv: list[str] | None = None) -> int:
    global _COMPILER, _TARGET, _CLANG_INC, _SYSROOT, _NM, _LIBC, _MANIFEST
    global _BIN_CMAKE, _BOOT_TAR, _REPORT
    parser = argparse.ArgumentParser(
        description="Run Gate 2 POSIX.1-2024 AST and symbol diff check."
    )
    parser.add_argument("--cc", default="clang")
    parser.add_argument("--target", default="x86_64-unknown-none-elf")
    parser.add_argument("--clang-include", default=None)
    parser.add_argument("--sysroot", default=str(DEFAULT_SYSROOT))
    parser.add_argument("--nm", default="llvm-nm")
    parser.add_argument("--libc", default=str(DEFAULT_LIBC))
    parser.add_argument("--manifest", default=str(DEFAULT_MANIFEST))
    parser.add_argument("--bin-cmake", default=str(DEFAULT_BIN_CMAKE))
    parser.add_argument("--boot-tar", default=None)
    parser.add_argument("--report", default=None)
    args, _ = parser.parse_known_args(argv)

    _COMPILER = args.cc
    _TARGET = args.target
    _CLANG_INC = Path(args.clang_include) if args.clang_include else None
    _SYSROOT = Path(args.sysroot)
    _NM = args.nm or "llvm-nm"
    _LIBC = Path(args.libc)
    _MANIFEST = Path(args.manifest)
    _BIN_CMAKE = Path(args.bin_cmake)
    _BOOT_TAR = Path(args.boot_tar) if args.boot_tar else None
    _REPORT = Path(args.report) if args.report else None

    suite = unittest.defaultTestLoader.loadTestsFromTestCase(
        PosixSymbolDiffTests
    )
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
