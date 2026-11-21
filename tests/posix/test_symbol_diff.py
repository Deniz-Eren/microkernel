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
    BASE_CONFORMANCE_GROUPS,
    OPTION_GROUP_MACROS,
    PosixManifest,
    detect_clang_include,
    extract_archive_symbols,
    extract_defined_macros,
    extract_header_ast_symbols,
    is_reserved_identifier,
    load_manifest,
)

DEFAULT_SYSROOT = ROOT / "sysroot" / "usr" / "include"
DEFAULT_LIBC = ROOT / "sysroot" / "usr" / "lib" / "libc.a"
DEFAULT_MANIFEST = ROOT / "tests" / "posix" / "issue8-manifest.tsv"

IMPLEMENTED_POSIX_HEADERS: tuple[str, ...] = (
    "aio.h",
    "dirent.h",
    "errno.h",
    "fcntl.h",
    "poll.h",
    "pthread.h",
    "semaphore.h",
    "signal.h",
    "stdalign.h",
    "stdatomic.h",
    "stdio.h",
    "stdlib.h",
    "string.h",
    "time.h",
    "uchar.h",
    "unistd.h",
    "sys/mman.h",
    "sys/stat.h",
    "sys/types.h",
    "sys/wait.h",
)

_COMPILER = "clang"
_CLANG_INC: Path | None = None
_SYSROOT = DEFAULT_SYSROOT
_NM = "llvm-nm"
_LIBC = DEFAULT_LIBC
_MANIFEST = DEFAULT_MANIFEST
_REPORT: Path | None = None


def _is_option_allowed(group: str, active_macros: set[str]) -> bool:
    if group in BASE_CONFORMANCE_GROUPS:
        return True
    macros = OPTION_GROUP_MACROS.get(group, ())
    return any(m in active_macros for m in macros)


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
            if not _is_option_allowed(entry.group, active_macros):
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
    active_macros: set[str],
) -> None:
    base_hdrs = manifest.base_headers()
    impl_hdrs = set(IMPLEMENTED_POSIX_HEADERS) & base_hdrs
    missing_hdrs = sorted(base_hdrs - impl_hdrs)

    base_ifaces = manifest.base_interfaces()
    impl_ifaces = archive_symbols & base_ifaces
    missing_ifaces = sorted(base_ifaces - impl_ifaces)
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
        "# POSIX.1-2024 (Issue 8) Symbol Diff Report",
        f"headers_base_implemented: {len(impl_hdrs)}/{len(base_hdrs)}",
        f"interfaces_base_implemented: {len(impl_ifaces)}/{len(base_ifaces)}",
        f"header_ast_declarations: {len(ast_symbols)}",
        f"libc_exported_symbols: {len(archive_symbols)}",
        f"active_option_groups: {', '.join(active_opt_groups) or 'none'}",
        f"reserved_internal_exports: {', '.join(reserved_exports) or 'none'}",
        "",
        "## Missing BASE/CX/OB Headers",
        *missing_hdrs,
        "",
        "## Missing BASE/CX/OB Interfaces",
        *missing_ifaces,
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

        clang_inc = _CLANG_INC or detect_clang_include(_COMPILER)
        active_macros = extract_defined_macros(
            _COMPILER, clang_inc, _SYSROOT, "unistd.h"
        )
        self.assertIn("_POSIX_VERSION", active_macros)

        ast_symbols = extract_header_ast_symbols(
            _COMPILER, clang_inc, _SYSROOT, IMPLEMENTED_POSIX_HEADERS
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
            _write_diff_report(
                _REPORT, manifest, ast_symbols, archive_symbols, active_macros
            )
            self.assertTrue(_REPORT.is_file())


def main(argv: list[str] | None = None) -> int:
    global _COMPILER, _CLANG_INC, _SYSROOT, _NM, _LIBC, _MANIFEST, _REPORT
    parser = argparse.ArgumentParser(
        description="Run Gate 2 POSIX.1-2024 AST and symbol diff check."
    )
    parser.add_argument("--cc", default="clang")
    parser.add_argument("--clang-include", default=None)
    parser.add_argument("--sysroot", default=str(DEFAULT_SYSROOT))
    parser.add_argument("--nm", default="llvm-nm")
    parser.add_argument("--libc", default=str(DEFAULT_LIBC))
    parser.add_argument("--manifest", default=str(DEFAULT_MANIFEST))
    parser.add_argument("--report", default=None)
    args, _ = parser.parse_known_args(argv)

    _COMPILER = args.cc
    _CLANG_INC = Path(args.clang_include) if args.clang_include else None
    _SYSROOT = Path(args.sysroot)
    _NM = args.nm or "llvm-nm"
    _LIBC = Path(args.libc)
    _MANIFEST = Path(args.manifest)
    _REPORT = Path(args.report) if args.report else None

    suite = unittest.defaultTestLoader.loadTestsFromTestCase(
        PosixSymbolDiffTests
    )
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
