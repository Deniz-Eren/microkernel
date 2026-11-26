---
name: state-management
description: >-
  Procedures to change the traceable state documents of the project:
  docs/records/TASKS.md, docs/records/ROADMAP.md, docs/records/HISTORY.md,
  decision records (ADR_, BDR_, WDR_), CHANGELOG.md, and
  docs/THIRD-PARTY-NOTICES.md. Use this skill before each commit, when you
  complete a task, make a decision, or add a third-party file or dependency.
---

# State Management

All traceable state documents are in the Sphinx-Needs model in `docs/`. Legal
third-party attributions are in `docs/THIRD-PARTY-NOTICES.md`, and release
summaries are in `CHANGELOG.md`. Do not use an assigned ID again or change its
name. Write dates in ISO format (`YYYY-MM-DD`).

## Tasks, Roadmap, and History (`docs/records/`)

- **Add a task:** Add a `{task}` directive to the end of
  [docs/records/TASKS.md](../../records/TASKS.md) with the next `TSK_NNNN` ID,
  `:status: open`, and `:traces:` links.
- **Add a roadmap item:** Add a `{roadmap}` directive to the end of
  [docs/records/ROADMAP.md](../../records/ROADMAP.md) with the next `RDM_NNNN`
  ID.
- **Complete or cancel an item:** Move the `{task}` or `{roadmap}` directive
  unchanged to the top of [docs/records/HISTORY.md](../../records/HISTORY.md)
  and set `:status: done` (or `cancelled`). Normative items must not connect to
  `TSK_` or `RDM_` IDs.

## Decision Records (`ADR_`, `BDR_`, `WDR_`)

Add a numbered decision file when you make, change, or supersede an important
architecture, project, or workflow decision (with tool configurations and
deviations from conventions):

- **Architecture decisions:** `docs/30-architecture/adr/ADR_NNNN.md` (`{adr}`)
- **Business decisions:** `docs/10-product/bdr/BDR_NNNN.md` (`{bdr}`, must have
  `:drives:` or `:governs:`)
- **Workflow and tool decisions:** `docs/00-overview/wdr/WDR_NNNN.md` (`{wdr}`,
  usually has `:governs: ARC_DOCS_0001`)

1. Find the highest sequential 4-digit ID in the target directory and add 1.
2. Make `<PREFIX>_NNNN.md` with sections `## Context`, `## Decision`, `##
   Rejected alternatives`, and `## Results` in the directive.
3. To supersede a previous decision, set the previous file to `:status:
   superseded` and add `:supersedes: <OLD_ID>` on the new decision.
4. Index files (`index.md`) use `:glob:`, which lets Sphinx include new files
   automatically.

## CHANGELOG.md

The [keepachangelog skill](../keepachangelog/SKILL.md) gives the procedure for
`CHANGELOG.md`. Put each important change into its `## [Unreleased]` section.

## THIRD-PARTY-NOTICES.md

- For a **copied file or snippet**, add a Template A entry to Section 1 of
  [docs/THIRD-PARTY-NOTICES.md](../../THIRD-PARTY-NOTICES.md).
- For a **package dependency**, add a Template B entry to Section 2.
- Include the source URL and the full license text.

## Before each commit

1. Change [docs/records/TASKS.md](../../records/TASKS.md) and move completed
   items to [docs/records/HISTORY.md](../../records/HISTORY.md).
2. Add a `CHANGELOG.md` entry in `## [Unreleased]` for an important change
   (refer to the [keepachangelog skill](../keepachangelog/SKILL.md)).
3. If you made a new decision, add an `ADR_`, `BDR_`, or `WDR_` file.
4. If an active project convention changed, change
   [docs/records/CONVENTIONS.md](../../records/CONVENTIONS.md).
5. If you added a third-party component, add a notice in
   [docs/THIRD-PARTY-NOTICES.md](../../THIRD-PARTY-NOTICES.md).
6. Make sure that all new and changed source files have the necessary Doxygen
   and MIT copyright headers (refer to
   [docs/records/CONVENTIONS.md](../../records/CONVENTIONS.md), `CNV_0003`).
