# Interfaces — POWER

Domain `POWER`: Status and fault reporting interface contracts.

```{icd} Power status telemetry interface
:id: ICD_POWER_0001
:status: active
:refines: ARC_POWER_0001
:satisfies: SWR_POWER_0001

The power status interface publishes rail voltage readings and active fault
flags produced by the power monitor subsystem ({need}`ARC_POWER_0001`) to
satisfy the supply monitor requirement ({need}`SWR_POWER_0001`).
```
