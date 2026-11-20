# Architecture — POWER

Domain `POWER`: Structural architecture elements for power supply supervision.

```{arc} Power monitor subsystem
:id: ARC_POWER_0001
:status: active
:satisfies: SWR_POWER_0001
:traces: ADR_0001

The power monitor subsystem encapsulates voltage sampling, threshold evaluation,
and fault reporting to satisfy the supply monitor requirement
({need}`SWR_POWER_0001`).
```
