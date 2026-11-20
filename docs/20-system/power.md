# System and Software Requirements — POWER

Domain `POWER`: Power supply supervision and threshold monitoring requirements.

```{sys} Monitor primary power supply
:id: SYS_POWER_0001
:status: active

The system shall continuously monitor primary power supply rails and report
out-of-range conditions.
```

````{swr} Monitor supply voltage bounds
:id: SWR_POWER_0001
:status: active
:derives: SYS_POWER_0001

The supply monitor shall evaluate sampled rail voltage against configured lower
and upper limits derived from the primary power supply requirement
({need}`SYS_POWER_0001`).

```{swr} Detect low-voltage faults
:id: SWR_POWER_0002
:status: active
:derives: SWR_POWER_0001

The supply monitor shall raise a low-voltage fault whenever the measured rail
voltage drops below the configured minimum threshold.
```

```{swr} Detect high-voltage faults
:id: SWR_POWER_0003
:status: active
:derives: SWR_POWER_0001

The supply monitor shall raise a high-voltage fault whenever the measured rail
voltage exceeds the configured maximum threshold.
```
````
