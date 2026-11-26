# Components — DOCS

The `DOCS` domain includes internal specifications for the traceability model
auditor and two-direction code marker scanner.

```{comp} Sphinx-Needs model auditor
:id: COMP_DOCS_0001
:status: active
:refines: ARC_DOCS_0001, ICD_DOCS_0001
:satisfies: SWR_DOCS_0001, SWR_DOCS_0006

`tools/traceability/check_needs_audit.py` reads `needs.json` and examines
two-direction relations, relation rules, coverage, markers, and file names.
`tools/check_ste.py` starts the optional `ste-writing-skill` check across
`README.md` and `docs/`, and skips the check when the skill is missing. The
CTest boundary check examines Ring 0 modules, syscalls, and FD slots against
accepted ADRs ({need}`ICD_DOCS_0001`, {need}`ARC_DOCS_0001`).
```

```{comp} Two-direction code marker scanner and Sphinx check extension
:id: COMP_DOCS_0002
:status: active
:refines: ARC_DOCS_0001, ICD_DOCS_0001
:satisfies: SWR_DOCS_0002

`config/sphinx/tag_scan.py` and `config/sphinx/need_tag_check.py` examine
tracked files for `need::` markers and make `need-code-trace`. They stop the
build on incorrect markers, unmarked tests, or active `ICD_`/`COMP_`/`VER_`
items with no code markers ({need}`ICD_DOCS_0001`, {need}`ARC_DOCS_0001`).
```

```{comp} Shared POSIX.1-2024 manifest, AST, and call graph check library
:id: COMP_DOCS_0003
:status: active
:refines: ARC_DOCS_0001
:satisfies: SWR_DOCS_0003

`tools/posix/audit_lib.py` parses `tests/posix/issue8-manifest.tsv`, reads Clang
JSON AST declarations and `llvm-nm` exports, and makes `llvm-objdump` call graph
data for the POSIX.1-2024 compatibility gates ({need}`ARC_DOCS_0001`).
```

```{comp} Freestanding LLVM Raw Profile v8 collector and report tool
:id: COMP_DOCS_0004
:status: active
:refines: ARC_DOCS_0001, ICD_DOCS_0002
:satisfies: SWR_DOCS_0005

`kernel/core/cov.c` and `libc/src/cov_runtime.c` write `Raw Profile v8` data as
base64 frames through `arch_console_write()` with `-DMICROKERNEL_COVERAGE=ON`.
`tools/coverage/generate_report.py` parses frames and starts `llvm-profdata` and
`llvm-cov` to make HTML, LCOV, and text reports ({need}`ICD_DOCS_0002`,
{need}`ARC_DOCS_0001`).
```
