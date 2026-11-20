# AI Agent Governance

This file has the highest authority for AI agents that work in **microkernel**. The skill files in [docs/skills/](docs/skills/) have the procedures with more information. This file links to them, and it does not give the same information again.

---

## Commit Mode

`operations.commit_mode` in `.agent-scaffolding.yml` is the single source of truth for the commit mode and for the signature behavior of the commits. Read the mode from there. The [git skill](docs/skills/git/SKILL.md) defines the behavior of the modes and their typical work. This file does not define the modes.

---

## Predict → Act → Verify

1. **Predict:** examine the task, plan the steps, and find the possible failures before you act.
2. **Act:** do the planned steps. Make an atomic commit after each phase.
3. **Verify:** run the checks (build, tests, format, lint), and confirm that they are correct.

If verification is not correct, go back to Act, repair the problem, and verify again. Do not start the next phase until the current phase is correct.

---

## Failure Modes

| Failure | Step | If it continues |
|---|---|---|
| The build is not correct | Find the root cause. Repair the scaffolding (not the boilerplate). Run the build again. | After three attempts with no success, stop, and get the approval of the human |
| A test is not correct | Find the source of the failure, and repair it. Then run the tests again. | After three attempts with no success, stop, and get the approval of the human |
| The formatter or the linter does not accept its config | Remove one parameter at a time. Do a test again. Record an ADR. | If the config is not correct after that, use a config with no parameters, and record the uncertainty |
| A tool is not installed | Write in the skill of the tool that the tool must be installed. Record an ADR. | None |
| A git conflict | Stop the operation. Get the changes from the remote. Resolve the conflict. Attempt it again. | Put the decision to the human |
| The context or token limit is close | Give a summary of the earlier context. Link to files. Do not copy their content. | None |

---

## Boundaries for CLI Commands

| Command | Permitted | Approval necessary |
|---|---|---|
| Read files. Run `--version` checks. | ✓ | — |
| Run build, test, and formatter scripts | ✓ | — |
| Commit | the commit mode in `.agent-scaffolding.yml` gives the rules | — |
| Install the project dependencies in the development environment | ✓ | — |
| Install system packages | ✗ | ✓ |
| Erase project files | ✗ | ✓ |
| Change IMMUTABLE documents | ✗ | ✗ (not permitted) |
| Push to a remote | ✗ | ✓ |

**Build Agent First:** delegate all build, test, and format runs to the subagents in [docs/agents/](docs/agents/). If the platform has no subagents, run the scripts. Report only the summary line and the failures.

---

## Token and Context Management

- Load a skill only when it is necessary for the task.
- Link to skill files and documents. Do not copy their content.
- Give a summary of the earlier context. Do not give it again.
- Read connected files in one batch.
- Write `~` or `$HOME`. Do not write an absolute home path.

---

## Git Protocol

Read the identity from `.agent-scaffolding.yml` (`identity.git_author_name`, `identity.git_author_email`). The value `current_user` means the identity of the current user. Take the name and the email from the git configuration of the environment (`git config user.name`, `git config user.email`). Give the identity inline for each commit. Do not change git config:

```bash
git -c user.name="<name>" -c user.email="<email>" commit -F <message-file>
```

Each commit message obeys Conventional Commits. It has an empty line and an `Assisted-by:` footer at the end. The footer gives the names of the agent and the model **of the current session**. Do not copy them from earlier commits:

```
<type>[scope]: <description>

Assisted-by: <AgentName>:<Model> [+ <Tool> ...]
```

For the full procedure, see the [git skill](docs/skills/git/SKILL.md).

---

## Pre-Commit Checklist

Before each commit, verify:

- [ ] The commit message obeys Conventional Commits, and it has the `Assisted-by:` footer
- [ ] The build and the tests are correct (if applicable), including CTest (`docs.html`, `docs.audit`, `posix.headers`)
- [ ] The formatter reports no findings (if applicable)
- [ ] All new or changed requirements and docs in `README.md` and `docs/` obey ASD-STE100 (checked with the `asd-ste100` skill, using `must` as the modal verb)
- [ ] All new or changed interfaces, components, and test files carry valid `need::` tags (`ICD_`, `COMP_`, `VER_`)
- [ ] You updated `docs/TASKS.md` (`[x] COMPLETE YYYY-MM-DD`)
- [ ] `CHANGELOG.md` has an entry in `## [Unreleased]` for the change
- [ ] `docs/DECISIONS.md` has an ADR for each new decision
- [ ] `docs/THIRD-PARTY-NOTICES.md` has attribution for each new third-party component
- [ ] All new and changed source files have the copyright header
- [ ] You did not change an IMMUTABLE document

For the update procedures, see the [state-management skill](docs/skills/state-management/SKILL.md).

---

## Rules for This Project

Rules that apply only to this project. Each rule links to the skill or the document with more information.

- All documentation and requirements in `README.md` and `docs/` (including Sphinx-Needs `{sys}`, `{swr}`, and specification directives) must follow ASD-STE100 Simplified Technical English (Issue 9). Always use the `asd-ste100` skill to write and check requirements, use `must` (not `shall`) as the requirement modal verb (`authoring.modal_verb: must` in `.docs-traceability-setup.yml`), and keep project terms in `technical_terms.txt` and `allowed_phrases.txt`. See [docs/CONVENTIONS.md](docs/CONVENTIONS.md) and [docs/records/CONVENTIONS.md](docs/records/CONVENTIONS.md) (`CNV_0002`).
- The project moves forward in milestones. Make one milestone at a time, and commit after each complete milestone. A milestone is complete only when its test gives exit code 0. See [docs/TASKS.md](docs/TASKS.md).
- The kernel and the C library are freestanding C17 with warnings as errors. The kernel runs with no C runtime. See [docs/skills/cmake/SKILL.md](docs/skills/cmake/SKILL.md).
- Follow the two-tier bidirectional code and POSIX.1-2024 traceability methodology: keep Sphinx-Needs items at the contract/header/subsystem level with `:posix:` references, tag headers with `ICD_`, implementations with `COMP_`, and tests (`test_*`) with `VER_`, and never create per-function Markdown boilerplate. See [docs/00-overview/wdr/WDR_0002.md](docs/00-overview/wdr/WDR_0002.md), [docs/30-architecture/adr/ADR_0003.md](docs/30-architecture/adr/ADR_0003.md), and [docs/records/CONVENTIONS.md](docs/records/CONVENTIONS.md) (`CNV_0001`, `CNV_0004`).
