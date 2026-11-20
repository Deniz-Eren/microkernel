# Interfaces — DOCS

Domain `DOCS`: Traceability configuration schema and exported model contract.

```{icd} Traceability configuration and needs.json contract
:id: ICD_DOCS_0001
:status: active
:refines: ARC_DOCS_0001
:satisfies: SWR_DOCS_0001, SWR_DOCS_0002

`.docs-traceability-setup.yml` and `config/sphinx/needs_model.py` define the
single schema for domains, ID patterns, permitted SysML v2 links, and test file
patterns consumed by the documentation subsystem ({need}`ARC_DOCS_0001`) to
satisfy model auditing ({need}`SWR_DOCS_0001`) and tag validation
({need}`SWR_DOCS_0002`).
```
