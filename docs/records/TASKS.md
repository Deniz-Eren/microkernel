# Tasks

## Purpose

Track active, temporary engineering tasks (`task`) for `microkernel`.

## Structure

Tasks are ordered by optimal execution order (highest priority first) and link
into the normative model through `:traces:`.

## Formatting Rules

- Normative specifications and records must not link to `TSK_` items.
- Move completed or cancelled tasks verbatim into [HISTORY.md](HISTORY.md) with
  status `done` or `cancelled`.

```{needtable} All tasks
:types: task
:filter: docname == 'records/TASKS'
:columns: id; title; status; traces as "Traces to"; traces_back as "Traced by"
```
