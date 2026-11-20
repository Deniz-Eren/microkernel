# Traceability Coverage

This page presents calculated traceability views generated directly from the
Sphinx-Needs model.

## Requirement Coverage (SYS, SWR, VER)

```{needtable} Requirement coverage
:types: sys, swr
:columns: id; title; status; derives_back; verifies_back as "Verified by"
```

## Unverified Requirements

Active requirements without a verification item (excluding `informational`
items) appear below. This table must be empty.

```{needtable} Unverified requirements
:types: sys, swr
:status: active
:filter: not verifies_back and 'informational' not in tags
:columns: id; title; status
```

## Orphan Verification Items

Active verification items without a `verifies` link appear below. This table
must be empty.

```{needtable} Orphan verification items
:types: ver
:status: active
:filter: not verifies
:columns: id; title; status
```

## Floating Design Items

Active architecture, interface, and component items without `satisfies` or
`refines` links appear below. This table must be empty.

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
