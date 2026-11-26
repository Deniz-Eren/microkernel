# Interfaces — DOCS

The `DOCS` domain includes the traceability configuration schema and exported
model interface.

```{icd} Traceability configuration and needs.json interface
:id: ICD_DOCS_0001
:status: active
:refines: ARC_DOCS_0001
:satisfies: SWR_DOCS_0001, SWR_DOCS_0002

`.docs-traceability-setup.yml` and `config/sphinx/needs_model.py` give the
schema for domains, ID patterns, SysML v2 relations, and test file patterns
({need}`ARC_DOCS_0001`, {need}`SWR_DOCS_0001`, {need}`SWR_DOCS_0002`).
```

```{icd} Kernel coverage collector and framed console ABI
:id: ICD_DOCS_0002
:status: active
:refines: ARC_DOCS_0001
:satisfies: SWR_DOCS_0005

`kernel/include/kernel/cov.h` and `SYS_COV_WRITE` (`COV_OP_REGISTER`,
`COV_OP_DUMP_ALL`, `struct __k_cov_desc`) give the optional coverage interface.
Base64 frames use `@@COV_BEGIN <name>@@`, `@@COV_END@@`, and `@@COV_COMPLETE@@`
through `arch_console_write()` ({need}`ARC_DOCS_0001`, {need}`SWR_DOCS_0005`).
```
