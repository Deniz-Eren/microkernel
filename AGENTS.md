# AI Agent Governance

This file has the highest authority for AI agents in **microkernel**. Skill
files in [docs/skills/](docs/skills/) hold full procedures. This file links to
them and does not repeat their content.

---

## Commit Mode

`operations.commit_mode` in `.agent-scaffolding.yml` is the single source of
truth for commit mode and commit signatures. Read the mode from there. The [git
skill](docs/skills/git/SKILL.md) defines each mode and its workflow.

---

## Predict -> Act -> Verify

1. **Predict:** examine the task, plan the steps, and find possible failures
   before you act.
2. **Act:** do the planned steps. Make an atomic commit after each phase.
3. **Verify:** run all checks (build, QEMU tests, CTest, `clang-format`,
   `clang-tidy`, `docs-traceability-setup` `check_text.py`, and `asd-ste100`)
   and confirm that every check passes.

If verification fails, go back to Act, repair the problem, and verify again. Do
not start the next phase until the current phase passes.

---

## Failure Modes

| Failure | Step | If it continues |
| --- | --- | --- |
| Build fails | Fix root cause and rebuild | After 3 tries, ask human |
| Test fails | Fix failure source and retest | After 3 tries, ask human |
| Config rejected | Drop 1 option, test, log ADR | Empty config, log risk |
| Tool missing | Note in tool skill, log ADR | None |
| Git conflict | Stop, pull, resolve, retry | Ask human |
| Near token limit | Summarize and link files | None |

---

## Boundaries for CLI Commands

| Command | Permitted | Approval |
| --- | --- | --- |
| Read files and `--version` checks | Yes | No |
| Run build, test, format, and lint scripts | Yes | No |
| Commit | See `commit_mode` in `.agent-scaffolding.yml` | No |
| Install project deps in dev container | Yes | No |
| Install system packages | No | Yes |
| Erase project files | No | Yes |
| Change IMMUTABLE documents | No | Not permitted |
| Push to a remote | No | Yes |

**Build Agent First:** delegate build, test, and format runs to the subagents in
[docs/agents/](docs/agents/). If the platform has no subagents, run the scripts.
Report only the summary line and failures.

---

## Token and Context Management

- Load a skill only when necessary for the task.
- Link to skill files and documents. Do not copy their content.
- Give a summary of earlier context. Do not repeat it.
- Read connected files in one batch.
- Write `~` or `$HOME`. Do not write an absolute home path.

---

## Git Protocol

Read identity from `.agent-scaffolding.yml` (`identity.git_author_name`,
`identity.git_author_email`). The value `current_user` uses the environment git
config (`git config user.name`, `git config user.email`). Pass the identity
inline for each commit and do not change git config:

```bash
git -c user.name="<name>" -c user.email="<email>" commit -F <message-file>
```

Each commit message obeys Conventional Commits with an empty line and an
`Assisted-by:` footer naming the agent and model of the **current session**:

```text
<type>[scope]: <description>

Assisted-by: <AgentName>:<Model> [+ <Tool> ...]
```

For the full procedure, see the [git skill](docs/skills/git/SKILL.md).

---

## Pre-Commit Checklist

Before each commit, verify:

- [ ] Commit message obeys Conventional Commits with `Assisted-by:` footer
- [ ] Build and tests pass, including QEMU and CTest (`docs.html`, `docs.audit`,
  `posix.headers`)
- [ ] Code formatters and linters pass (`clang-format`, `clang-tidy`)
- [ ] `.editorconfig` and `docs-traceability-setup` source-text checks pass on
  all new or changed files (`check_text.py --root .`: max line width, greedy
  prose packing, and Doxygen file headers)
- [ ] All new or changed requirements and docs in `README.md` and `docs/` obey
  ASD-STE100 Issue 9 (checked with the `asd-ste100` skill, modal verb `must`)
- [ ] All new or changed interfaces, components, and test files carry valid
  `need::` tags (`ICD_`, `COMP_`, `VER_`)
- [ ] `docs/TASKS.md` updated (`[x] COMPLETE YYYY-MM-DD`)
- [ ] `CHANGELOG.md` has an entry in `## [Unreleased]`
- [ ] `docs/DECISIONS.md` has an ADR for each new decision
- [ ] `docs/THIRD-PARTY-NOTICES.md` attributes each new third-party component
- [ ] All new and changed source files have the copyright header
- [ ] No IMMUTABLE document was changed

For update procedures, see
[state-management](docs/skills/state-management/SKILL.md).

---

## Rules for This Project

Rules that apply only to this project. Each rule links to its skill or document.

- **Source-text and `.editorconfig` compliance (`docs-traceability-setup`):**
  All new and changed files must obey `.editorconfig` (UTF-8, LF, trailing
  newline, max line width) and pass the `docs-traceability-setup` skill's
  `scripts/check_text.py` (maximum line length, greedy prose packing, and
  Doxygen file headers) at all times alongside `clang-format`, `clang-tidy`,
  `asd-ste100`, `posix-issue8-os-dev`, and CTest. Repair content rather than
  weakening rules. See `.editorconfig`, `.docs-traceability-setup.yml`, and
  [docs/records/CONVENTIONS.md](docs/records/CONVENTIONS.md) (`CNV_0002`,
  `CNV_0003`).
- **ASD-STE100 technical English (`asd-ste100`):** All documentation and
  requirements in `README.md` and `docs/` (including Sphinx-Needs `{sys}`,
  `{swr}`, and specification directives) must follow ASD-STE100 Issue 9. Always
  use the `asd-ste100` skill to write and check requirements, use `must` (not
  `shall`) as the modal verb (`authoring.modal_verb: must`), and keep project
  terms in `technical_terms.txt` and `allowed_phrases.txt`. See
  [docs/CONVENTIONS.md](docs/CONVENTIONS.md) and
  [docs/records/CONVENTIONS.md](docs/records/CONVENTIONS.md) (`CNV_0002`).
- **Two-tier bidirectional code and POSIX traceability
  (`posix-issue8-os-dev`):** Keep Sphinx-Needs items at the contract, header, or
  subsystem level with `:posix:` references; tag headers with `ICD_`,
  implementations with `COMP_`, and tests (`test_*`) with `VER_`; never create
  per-function Markdown boilerplate. See
  [WDR_0002](docs/00-overview/wdr/WDR_0002.md),
  [ADR_0003](docs/30-architecture/adr/ADR_0003.md), and
  [docs/records/CONVENTIONS.md](docs/records/CONVENTIONS.md) (`CNV_0001`,
  `CNV_0004`).
- **Milestone discipline:** Make one milestone at a time and commit after each
  complete milestone. A milestone is complete only when its test gives exit code
  0. See [docs/TASKS.md](docs/TASKS.md).
- **Freestanding C17:** The kernel and C library are freestanding C17 with
  warnings as errors. The kernel runs with no C runtime. See
  [docs/skills/cmake/SKILL.md](docs/skills/cmake/SKILL.md).
