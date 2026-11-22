##
# @file     audit_lib.py
# @brief    Shared POSIX.1-2024 manifest, AST, nm, and call-graph audit helpers
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Shared helpers for POSIX.1-2024 symbol diff, safety, and cancel gates."""

# need:: COMP_DOCS_0003

from __future__ import annotations

import json
import re
import subprocess
import tempfile
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence

BASE_TARGET_GROUPS: frozenset[str] = frozenset({"BASE", "CX", "OB"})

OPTION_GROUP_MACROS: dict[str, tuple[str, ...]] = {
    "ADV": ("_POSIX_ADVISORY_INFO",),
    "CPT": ("_POSIX_CPUTIME",),
    "DC": ("_POSIX_DEVICE_CONTROL",),
    "FSC": ("_POSIX_FSYNC",),
    "IP6": ("_POSIX_IPV6",),
    "ML": ("_POSIX_MEMLOCK",),
    "MLR": ("_POSIX_MEMLOCK_RANGE",),
    "MSG": ("_POSIX_MESSAGE_PASSING",),
    "PS": ("_POSIX_PRIORITY_SCHEDULING",),
    "SHM": ("_POSIX_SHARED_MEMORY_OBJECTS",),
    "SIO": ("_POSIX_SYNCHRONIZED_IO",),
    "SPN": ("_POSIX_SPAWN",),
    "TCT": ("_POSIX_THREAD_CPUTIME",),
    "TPS": ("_POSIX_THREAD_PRIORITY_SCHEDULING",),
    "TSH": ("_POSIX_THREAD_PROCESS_SHARED",),
    "TSS": ("_POSIX_THREAD_ATTR_STACKSIZE",),
    "TYM": ("_POSIX_TYPED_MEMORY_OBJECTS",),
    "XSI": ("_XOPEN_UNIX",),
}

_FUNC_HDR_RE = re.compile(r"^[0-9a-fA-F]+\s+<([^>]+)>:\s*$")
_CALL_RE = re.compile(
    r"\b(?:callq?|jmpq?|bl|b)\s+.*<([^+>]+)(?:\+0x[0-9a-fA-F]+)?>"
)
_RELOC_RE = re.compile(
    r"\bR_(?:X86_64|AARCH64)_\w+\s+([A-Za-z_][A-Za-z0-9_.$]*)"
)
_DEFINE_RE = re.compile(r"^#define\s+([A-Za-z_][A-Za-z0-9_]*)\b")


@dataclass(frozen=True)
class ManifestEntry:
    """Single identifier row parsed from `issue8-manifest.tsv`."""

    volume: str
    identifier: str
    category: str
    group: str
    url: str


@dataclass(frozen=True)
class PosixManifest:
    """Indexed view of POSIX.1-2024 Issue 8 headers and interfaces."""

    headers: dict[str, ManifestEntry]
    interfaces: dict[str, ManifestEntry]

    def base_headers(self) -> set[str]:
        return {
            name
            for name, entry in self.headers.items()
            if entry.group in BASE_TARGET_GROUPS
        }

    def base_interfaces(self) -> set[str]:
        return {
            name
            for name, entry in self.interfaces.items()
            if entry.group in BASE_TARGET_GROUPS
        }


def load_manifest(path: Path) -> PosixManifest:
    """Parse `issue8-manifest.tsv` into header and interface maps."""
    headers: dict[str, ManifestEntry] = {}
    interfaces: dict[str, ManifestEntry] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        parts = line.split("\t")
        if len(parts) < 5:
            continue
        entry = ManifestEntry(
            volume=parts[0],
            identifier=parts[1],
            category=parts[2],
            group=parts[3],
            url=parts[4],
        )
        if entry.category == "header":
            headers[entry.identifier] = entry
        elif entry.category == "interface":
            interfaces[entry.identifier] = entry
    return PosixManifest(headers=headers, interfaces=interfaces)


def is_reserved_identifier(name: str) -> bool:
    """Return True if `name` uses the reserved `__` or `_[A-Z]` prefix."""
    if name.startswith("__"):
        return True
    return len(name) >= 2 and name[0] == "_" and name[1].isupper()


def detect_clang_include(cc: str) -> Path:
    """Locate Clang's freestanding resource include directory."""
    proc = subprocess.run(
        [cc, "-print-resource-dir"],
        check=True,
        capture_output=True,
        text=True,
    )
    return Path(proc.stdout.strip()) / "include"


def extract_defined_macros(
    cc: str,
    clang_inc: Path,
    sysroot: Path,
    header: str,
    feature_macro: str = "-D_POSIX_C_SOURCE=202405L",
    target: str = "x86_64-unknown-none-elf",
) -> set[str]:
    """Return macro names defined after preprocessing `#include <header>`."""
    return extract_headers_defined_macros(
        cc,
        clang_inc,
        sysroot,
        (header,),
        feature_macro=feature_macro,
        target=target,
    )


def extract_headers_defined_macros(
    cc: str,
    clang_inc: Path,
    sysroot: Path,
    headers: Sequence[str],
    feature_macro: str = "-D_POSIX_C_SOURCE=202405L",
    target: str = "x86_64-unknown-none-elf",
) -> set[str]:
    """Return macro names defined across all `#include <hdr>` headers."""
    src = "".join(f"#include <{hdr}>\n" for hdr in headers)
    with tempfile.NamedTemporaryFile(
        mode="w", suffix=".c", encoding="utf-8", delete=True
    ) as tmp:
        tmp.write(src)
        tmp.flush()
        proc = subprocess.run(
            [
                cc,
                "-target",
                target,
                "-ffreestanding",
                "-nostdinc",
                "-isystem",
                str(sysroot),
                "-isystem",
                str(clang_inc),
                "-std=c17",
                feature_macro,
                "-dM",
                "-E",
                tmp.name,
            ],
            check=True,
            capture_output=True,
            text=True,
        )
    macros: set[str] = set()
    for line in proc.stdout.splitlines():
        m = _DEFINE_RE.match(line)
        if m:
            macros.add(m.group(1))
    return macros


def extract_header_ast_symbols(
    cc: str,
    clang_inc: Path,
    sysroot: Path,
    headers: Sequence[str],
    feature_macro: str = "-D_POSIX_C_SOURCE=202405L",
    target: str = "x86_64-unknown-none-elf",
) -> set[str]:
    """Extract top-level FunctionDecl and VarDecl names via Clang JSON AST."""
    src = "".join(f"#include <{hdr}>\n" for hdr in headers)
    with tempfile.NamedTemporaryFile(
        mode="w", suffix=".c", encoding="utf-8", delete=True
    ) as tmp:
        tmp.write(src)
        tmp.flush()
        proc = subprocess.run(
            [
                cc,
                "-target",
                target,
                "-ffreestanding",
                "-nostdinc",
                "-isystem",
                str(sysroot),
                "-isystem",
                str(clang_inc),
                "-std=c17",
                feature_macro,
                "-Xclang",
                "-ast-dump=json",
                "-fsyntax-only",
                tmp.name,
            ],
            check=True,
            capture_output=True,
            text=True,
        )
    ast = json.loads(proc.stdout)
    symbols: set[str] = set()
    for node in ast.get("inner", ()):
        if node.get("isImplicit"):
            continue
        if node.get("kind") in ("FunctionDecl", "VarDecl"):
            name = node.get("name", "")
            if name:
                symbols.add(name)
    return symbols


def extract_archive_symbols(nm: str, libc_a: Path) -> set[str]:
    """Return globally defined symbols in `libc.a` via `llvm-nm`."""
    proc = subprocess.run(
        [nm, "--defined-only", "-g", str(libc_a)],
        check=True,
        capture_output=True,
        text=True,
    )
    exported: set[str] = set()
    for line in proc.stdout.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[1] in ("T", "D", "B", "R", "W"):
            exported.add(parts[2])
    return exported


def build_call_graph(
    objdump: str, libc_a: Path
) -> tuple[set[str], dict[str, set[str]]]:
    """Disassemble `libc.a` with relocations to build a static call graph."""
    proc = subprocess.run(
        [objdump, "-dr", str(libc_a)],
        check=True,
        capture_output=True,
        text=True,
    )
    defined: set[str] = set()
    edges: dict[str, set[str]] = defaultdict(set)
    current: str | None = None
    for line in proc.stdout.splitlines():
        m_fn = _FUNC_HDR_RE.match(line)
        if m_fn:
            current = m_fn.group(1)
            defined.add(current)
            continue
        if current is None:
            continue
        m_reloc = _RELOC_RE.search(line)
        if m_reloc:
            sym = m_reloc.group(1)
            if sym != current and not sym.startswith("."):
                edges[current].add(sym)
            continue
        m_call = _CALL_RE.search(line)
        if m_call:
            callee = m_call.group(1)
            if callee != current:
                edges[current].add(callee)
    return defined, edges


def reachable(start: str, edges: dict[str, set[str]]) -> set[str]:
    """Return all symbols reachable from `start` in the static call graph."""
    seen: set[str] = set()
    stack = [start]
    while stack:
        fn = stack.pop()
        for nxt in edges.get(fn, ()):
            if nxt not in seen:
                seen.add(nxt)
                stack.append(nxt)
    return seen
