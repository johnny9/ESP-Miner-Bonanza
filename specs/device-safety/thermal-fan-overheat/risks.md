# Thermal, fan, and overheat control — risks and scope

## Scope and review unit

- **In:** Sensor authority, fan modes/clamps, overheat priority, mining
  stop/start, ASIC frequency/reset boundary, recovery, and evidence.
- **Out:** Tuning UI layout and ASIC algorithm internals behind driver callbacks.
- **Smallest coherent change:** Priority and lifecycle changes span power, fan,
  thermal, ASIC, state, API, and tests and must move atomically for safety.
- **Split out:** Board-specific driver implementations can be reviewed separately
  once the capability contract is fixed.

## Assumptions and open questions

- Define fail-safe behavior when all temperature sensors are unavailable while
  mining for each board family.
- Replace fixed two-sensor consumers with a bounded sensor collection.
- Move overheat evaluation ahead of pause/hardware-fault early continuation.

## Failure modes

| Failure | Impact | Detection | Mitigation |
|---|---|---|---|
| Pause masks overheat | Hardware remains hot without recovery state | Priority unit test/HIL | Evaluate safety first |
| Invalid sensor drives PID | Low/high erroneous fan command | Sentinel/non-finite tests | Ignore or fail safe |
| Fan write fails silently | Loss of cooling | Driver failure injection/RPM | Hardware fault and safe state |
| Duplicate restart path | UART/power race | Lifecycle regression | One shared init/reset path |
| Model fact in generic task | New board unsafe | Architecture review | Capability API owner |

## Security, resources, and rollout

- Treat settings and telemetry as untrusted until validated; no remote request
  may bypass an active safety state.
- Preserve task cadence, bound waits, and avoid repeated flash writes.
- Roll back only to firmware known safe for the exact board, with current
  settings and overheat marker interpretable by that version.
