# History

## Purpose

Archive completed and cancelled planning entries (`task`, `roadmap`) moved from
[TASKS.md](TASKS.md) and [ROADMAP.md](ROADMAP.md).

## Structure

Entries are stored newest first without modifying their original IDs or links.

## Formatting Rules

- Set the status of every archived item to `done` or `cancelled`.
- Never reuse IDs from archived entries.

```{needtable} Completed tasks and roadmap items
:types: task, roadmap
:filter: docname == 'records/HISTORY' and status in ['done', 'cancelled']
:columns: id; title; status; traces as "Traces to"; traces_back as "Traced by"
```
