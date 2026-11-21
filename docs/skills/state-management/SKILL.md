---
name: state-management
description: >-
  Procedures to update the traceable state documents of the project:
  docs/records/TASKS.md, docs/records/ROADMAP.md, docs/records/HISTORY.md,
  decision records (ADR_, BDR_, WDR_), CHANGELOG.md, and
  docs/THIRD-PARTY-NOTICES.md. Use this skill before each commit, when you
  complete a task, make a decision, or add a third-party file or dependency.
---

# State Management

All traceable state documents live in the Sphinx-Needs model under `docs/`.
Legal third-party attributions live in `docs/THIRD-PARTY-NOTICES.md`, and
release notes live in `CHANGELOG.md`. Never reuse or rename an assigned ID.
Write dates in ISO format (`YYYY-MM-DD`).

## Tasks, Roadmap, and History (`docs/records/`)

- **Add a task:** Append a `{task}` directive with the next `TSK_NNNN` ID,
  `:status: open`, and `:traces:` links in
  [docs/records/TASKS.md](../../records/TASKS.md).
- **Add a roadmap item:** Append a `{roadmap}` directive with the next
  `RDM_NNNN` ID in [docs/records/ROADMAP.md](../../records/ROADMAP.md).
- **Complete or cancel an item:** Move the `{task}` or `{roadmap}` directive
  verbatim to the top of [docs/records/HISTORY.md](../../records/HISTORY.md) and
  set `:status: done` (or `cancelled`). Normative items must never link to
  `TSK_` or `RDM_` IDs.

## Decision Records (`ADR_`, `BDR_`, `WDR_`)

Add a numbered decision file whenever you make, change, or supersede an
important architectural, product, or workflow decision (including tool configs
and deviations from conventions):

- **Architecture decisions:** `docs/30-architecture/adr/ADR_NNNN.md` (`{adr}`)
- **Business decisions:** `docs/10-product/bdr/BDR_NNNN.md` (`{bdr}`, must carry
  `:drives:` or `:governs:`)
- **Workflow and tooling decisions:** `docs/00-overview/wdr/WDR_NNNN.md`
  (`{wdr}`, typically carries `:governs: ARC_DOCS_0001`)

1. Find the highest sequential 4-digit ID in the target folder and add 1.
2. Create `<PREFIX>_NNNN.md` with sections `## Context`, `## Decision`, `##
   Rejected alternatives`, and `## Consequences` inside the directive.
3. To supersede an old decision, set the old file to `:status: superseded` and
   add `:supersedes: <OLD_ID>` on the new decision.
4. Indexes (`index.md`) use `:glob:`, so Sphinx includes new files
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

1. Update [docs/records/TASKS.md](../../records/TASKS.md) and move completed
   items to [docs/records/HISTORY.md](../../records/HISTORY.md).
2. Add a `CHANGELOG.md` entry in `## [Unreleased]` for an important change (see
   the [keepachangelog skill](../keepachangelog/SKILL.md)).
3. Add an `ADR_`, `BDR_`, or `WDR_` file if you made a new decision.
4. Update [docs/records/CONVENTIONS.md](../../records/CONVENTIONS.md) if a
   standing engineering convention changed.
5. Add a notice in [docs/THIRD-PARTY-NOTICES.md](../../THIRD-PARTY-NOTICES.md)
   if you added a third-party component.
6. Confirm all new and changed source files carry the required Doxygen and MIT
   copyright headers (see
   [docs/records/CONVENTIONS.md](../../records/CONVENTIONS.md), `CNV_0003`).
