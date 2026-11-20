# Roadmap

## Purpose

Track planned milestones and roadmap entries (`roadmap`) for `microkernel`.

## Structure

Order roadmap items by priority and link them to affected specifications through
`:traces:`.

## Formatting Rules

- Normative specifications and records must not link to `RDM_` items.
- Move completed or cancelled roadmap items verbatim into
  [HISTORY.md](HISTORY.md) with status `done` or `cancelled`.

```{needtable} All roadmap items
:types: roadmap
:filter: docname == 'records/ROADMAP'
:columns: id; title; status; traces as "Traces to"; traces_back as "Traced by"
```
