---
name: git
description: >-
  Git conventions for this project, with commit identity, Conventional Commits
  messages, the Assisted-by footer, commit modes, and branch rules. Use this
  skill when you make commits, write commit messages, tag releases, or manage
  branches.
---

# Git

## Commit mode

The commit mode is `operations.commit_mode` in `.agent-scaffolding.yml` at the
project root.

- **AUTONOMOUS (`--no-gpg-sign`):** The agent makes commits without a pause. Use
  this mode for usual maintenance and document changes.
- **HITL (default signature):** Write `WAITING FOR GPG SIGNATURE` and wait for
  the human. Use this mode for important changes, access-control changes, and
  releases.

## Identity

Read the identity from `.agent-scaffolding.yml` (`identity.git_author_name`,
`identity.git_author_email`). The value `current_user` sets the identity to the
active user. Get the name and email from the environment git configuration (`git
config user.name`, `git config user.email`). Give these values inline. Do not
start `git config` to change the identity:

```bash
git -c user.name="<name>" -c user.email="<email>" commit -F <message-file>
```

In AUTONOMOUS mode, add `--no-gpg-sign`.

## Message format

Obey [Conventional Commits](references/COMMITS.md):

```
<type>[(scope)][!]: <description>

[body]

Assisted-by: <AgentName>:<Model> [+ <Tool> ...]
```

- Types: `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `build`,
  `ci`, `chore`, `revert`, `scaffold`.
- For changes that are not compatible, put `!` after the type, or add a
  `BREAKING CHANGE:` footer.
- `Assisted-by:` is necessary for commits made with AI aid. It gives the names
  of the agent and model **of the active session**. Do not make a copy of the
  footer from previous commits.

Example:

```
feat(parser): support nested tables

Assisted-by: agent-name:model-name
```

## Workflow

1. Start `git status`, and put only the files for this change in the git index.
2. Examine the staged diff with `git diff --staged`.
3. Complete the pre-commit checklist in [AGENTS.md](../../../AGENTS.md). For
   state documents, refer to the [state-management
   skill](../state-management/SKILL.md).
4. Make a commit with the identity command in the Identity section.
5. Do not push without clear approval from the human.

## Versions and releases

- Obey [Semantic Versioning](references/semver.md). If the project has a build
  configuration, keep the version only there. If the project has no build
  configuration, keep the version in one file (for example `CHANGELOG.md` or a
  `VERSION` file).
- Tag releases as `vX.Y.Z`. Move the content of `## [Unreleased]` in
  [CHANGELOG.md](../../../CHANGELOG.md) into the new version section (refer to
  the [keepachangelog skill](../keepachangelog/SKILL.md)).

## Branches

- Branch protection is on for the default branch (usually `main`). Make your
  changes on topic branches that start from it.
- Squash-merge into the default branch. The squashed message must obey
  Conventional Commits.
