# Components — POWER

Domain `POWER`: White-box component specifications for supply monitoring.

```{comp} Power monitor evaluator
:id: COMP_POWER_0001
:status: active
:refines: ARC_POWER_0001
:satisfies: SWR_POWER_0001

The power monitor evaluator compares sampled rail voltages against configured
bounds and updates status telemetry for the power monitor subsystem
({need}`ARC_POWER_0001`).
```
