# Traceability Coverage

Sphinx-Needs calculates the traceability tables on this page directly from the
traceability model.

## POSIX.1-2024 Status Matrix

The table that follows shows all requirements that have a `:posix:` Issue 8
manifest entry, with their related design items and verification coverage. This
matrix records status for the objective of compatibility with interfaces from
POSIX.1-2024 (IEEE Std 1003.1-2024 / ISO/IEC/IEEE 9945:2026). This project does
not state that it has official certification. POSIX® is a registered trademark
of the Institute of Electrical and Electronic Engineers, Inc.

```{needtable} POSIX.1-2024 progress matrix
---
types: sys, swr
filter: posix
columns: >
  id; posix as "POSIX.1-2024 Ref"; title; status;
  satisfies_back as "Design (ICD/COMP)"; verifies_back as "Verified by"
---
```

## Requirement Coverage (SYS, SWR, VER)

```{needtable} Requirement coverage
:types: sys, swr
:columns: id; title; status; derives_back; verifies_back as "Verified by"
```

## Requirements Without Verification

The table that follows shows active requirements that have no verification item.
It does not include `informational` items. This table must be empty.

```{needtable} Unverified requirements
:types: sys, swr
:status: active
:filter: not verifies_back and 'informational' not in tags
:columns: id; title; status
```

## Verification Items Without Requirements

The table that follows shows active verification items that have no `verifies`
link. This table must be empty.

```{needtable} Orphan verification items
:types: ver
:status: active
:filter: not verifies
:columns: id; title; status
```

## Design Items Without Requirements

The table that follows shows active architecture, interface, and component items
that have no `satisfies` or `refines` link. This table must be empty.

```{needtable} Floating design items
:types: arc, icd, comp
:status: active
:filter: not satisfies and not refines
:columns: id; type; title; status
```

## Traceability Flow

````{only} has_graphviz
```{needflow}
:types: bdr, sys, swr, arc, icd, comp, ver
:show_link_names:
```
````
