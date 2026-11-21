##
# @file     readme_include.py
# @brief    Sphinx extension to include README.md on index and render Mermaid
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Include the repository README.md at the top of `docs/index.md`."""

# need:: COMP_DOCS_0002

from __future__ import annotations

import fnmatch
import html
import re
from pathlib import Path
from typing import Any

from docutils import nodes
from docutils.parsers.rst import Directive
from needs_model import CONFIG, REPO_ROOT
from sphinx.application import Sphinx

README_MARKER = "<!-- include-root-readme -->"
_FENCE_RE = re.compile(r"^\s*(`{3,}|~{3,})")
_LINK_RE = re.compile(r"(?<!!)\[([^\]]+)\]\(([^)\s]+)\)")
_EXTERNAL_PREFIXES = ("http://", "https://", "mailto:", "#")
_MERMAID_CDN = "https://cdn.jsdelivr.net/npm/mermaid@11/dist/mermaid.min.js"


def _is_excluded(rel_posix: str, exclude_patterns: list[str]) -> bool:
    parts = Path(rel_posix).parts
    for pat in exclude_patterns:
        clean = pat.rstrip("/")
        if fnmatch.fnmatch(rel_posix, clean):
            return True
        if parts and parts[0] == clean:
            return True
    return False


def _split_fenced_chunks(text: str) -> list[tuple[bool, str]]:
    """Split Markdown into `(is_code, chunk)` segments across code fences."""
    chunks: list[tuple[bool, str]] = []
    buf: list[str] = []
    in_fence = False
    fence_char = ""
    fence_len = 0

    for line in text.splitlines(keepends=True):
        m = _FENCE_RE.match(line)
        if not in_fence:
            if m:
                if buf:
                    chunks.append((False, "".join(buf)))
                    buf = []
                token = m.group(1)
                in_fence = True
                fence_char = token[0]
                fence_len = len(token)
                buf.append(line)
            else:
                buf.append(line)
        else:
            buf.append(line)
            if (
                m
                and m.group(1)[0] == fence_char
                and len(m.group(1)) >= fence_len
                and line.strip() == m.group(1)
            ):
                chunks.append((True, "".join(buf)))
                buf = []
                in_fence = False
                fence_char = ""
                fence_len = 0

    if buf:
        chunks.append((in_fence, "".join(buf)))
    return chunks


def rewrite_readme_for_docs(
    text: str,
    repo_root: Path,
    docs_dir: Path,
    exclude_patterns: list[str],
    repo_url: str = "",
) -> str:
    """Rewrite root-relative links in `README.md` for inclusion in `docs/`."""
    repo_resolved = repo_root.resolve()
    docs_resolved = docs_dir.resolve()
    base_url = repo_url.rstrip("/")

    def _replace_link(match: re.Match[str]) -> str:
        label = match.group(1)
        target = match.group(2)
        if target.startswith(_EXTERNAL_PREFIXES):
            return match.group(0)

        path_part, _, frag = target.partition("#")
        rel = path_part.removeprefix("./")
        candidate = (repo_resolved / rel).resolve()

        try:
            rel_to_docs = candidate.relative_to(docs_resolved)
        except ValueError:
            rel_to_docs = None

        if (
            rel_to_docs is not None
            and candidate.is_file()
            and candidate.suffix == ".md"
            and not _is_excluded(rel_to_docs.as_posix(), exclude_patterns)
        ):
            posix_rel = rel_to_docs.as_posix()
            if posix_rel == "index.md" and not frag:
                return f"[{label}](#traceability-documentation)"
            suffix = f"#{frag}" if frag else ""
            return f"[{label}]({posix_rel}{suffix})"

        if base_url:
            kind = "tree" if candidate.is_dir() else "blob"
            suffix = f"#{frag}" if frag else ""
            return f"[{label}]({base_url}/{kind}/main/{rel}{suffix})"

        clean_label = " ".join(label.split())
        if clean_label in (rel, path_part):
            return f"`{rel}`"
        return f"{label} (`{rel}`)"

    out: list[str] = []
    for is_code, chunk in _split_fenced_chunks(text):
        if is_code:
            out.append(chunk)
        else:
            out.append(_LINK_RE.sub(_replace_link, chunk))
    return "".join(out)


class MermaidDirective(Directive):
    """Render fenced Mermaid diagrams for HTML with plain-text fallback."""

    has_content = True

    def run(self) -> list[nodes.Node]:
        code = "\n".join(self.content)
        escaped = html.escape(code, quote=False)
        raw_html = (
            '<pre class="mermaid" '
            'style="background: transparent; border: none;">\n'
            f"{escaped}\n"
            "</pre>\n"
        )
        return [nodes.raw("", raw_html, format="html")]


def _inject_root_readme(
    app: Sphinx, docname: str, source: list[str]
) -> None:
    """Substitute the marker in `docs/index.md` with rewritten `README.md`."""
    if docname != "index" or README_MARKER not in source[0]:
        return

    readme_path = REPO_ROOT / "README.md"
    if not readme_path.is_file():
        return

    app.env.note_dependency(str(readme_path))
    raw_readme = readme_path.read_text(encoding="utf-8")
    repo_url = str(CONFIG["project"].get("repo_url", ""))
    rewritten = rewrite_readme_for_docs(
        raw_readme,
        repo_root=REPO_ROOT,
        docs_dir=Path(app.srcdir),
        exclude_patterns=list(app.config.exclude_patterns),
        repo_url=repo_url,
    )
    source[0] = source[0].replace(README_MARKER, rewritten.rstrip(), 1)


def setup(app: Sphinx) -> dict[str, Any]:
    """Register README inclusion on `index` and Mermaid HTML rendering."""
    app.add_directive("mermaid", MermaidDirective)
    app.connect("source-read", _inject_root_readme)
    app.add_js_file(_MERMAID_CDN, loading_method="defer")
    app.add_js_file(
        None,
        body=(
            "window.addEventListener('DOMContentLoaded', function () {"
            " if (window.mermaid) {"
            " window.mermaid.initialize({ startOnLoad: true });"
            " }"
            "});"
        ),
    )
    return {
        "version": "1.0.0",
        "parallel_read_safe": True,
        "parallel_write_safe": True,
    }
