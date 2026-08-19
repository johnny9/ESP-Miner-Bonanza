# ASIC driver contract — design

## Components and responsibilities

| Component | Responsibility | Implementation pointer |
|---|---|---|
| Public ASIC contract | Opaque instance, descriptor, lifecycle, work, event, telemetry, health, and status types | Current: `components/asic/include/asic.h`, `asic_driver.h`, `asic_capabilities.h`, `asic_work.h`, `asic_result.h`; target portable subset under `components/asic/include/` |
| Generic facade | Validate contract and dispatch instance operations | Current: `components/asic/asic.c`, `asic_driver.c`, `asic_capabilities.c` |
| Bitmain backend | BM detection, configuration, packets, slots, UART results, PLL/frequency mechanics | `components/asic/bm1397.c`, `bm1366.c`, `bm1368.c`, `bm1370.c`, `bm_*` helpers |
| BZM backend | Bridge/topology/lease/bring-up/dispatch/result/health mechanics | `components/asic/bzm*.c` |
| Board profile | Immutable model/count/topology/limits/qualification facts and required ports | Current source: `main/device_config.c`, `main/device_config.h`; target composition descriptor |
| Composition adapter | Construct the selected backend and inject power/thermal/serial/time/platform dependencies | Current split across `main/main.c`, `main/system.c`, `main/bzm_controller.c` |

## Authority and dependency direction

| Fact or state | Owner | Consumers | Forbidden duplicate |
|---|---|---|---|
| ASIC identity/topology/capabilities | Immutable ASIC descriptor backed by selected driver/profile | Mining, self-test, telemetry, API | Consumer model switch or failed-call capability probe |
| Driver lifecycle/fault | Opaque ASIC instance | Safety/composition/status | `ASIC_initalized` shadow Boolean as sole authority |
| Raw register/packet/slot state | Concrete backend | Backend implementation only | Stratum/self-test/result observer reading it |
| Approved operating request | Power/safety policy | ASIC set-operating-point operation | Driver choosing regulator/thermal policy |
| Canonical telemetry/health | Driver snapshot with timestamp/validity | Safety, self-test, API, monitoring | Consumer reading backend globals or magic sentinels |

## Target public contract

- `asic_device_t` is opaque and instance-scoped; no static singleton result or
  application-wide state is part of the interface.
- `asic_descriptor_t` contains stable identity, ASIC/work-unit topology,
  supported mining/diagnostic capabilities, and representable operating limits.
- `asic_start_purpose_t` distinguishes mining, self-test, and maintenance.
- `prepare/start/stop/destroy` define side effects, idempotence, safe state, and
  typed unsupported/fault/timeout/resource outcomes.
- Work and events use [the neutral mining pipeline](../../mining/work-pipeline/SPEC.md).
- Telemetry arrays are bounded, timestamped, and validity-aware. A feature
  profile owns pass/fail thresholds; the driver owns measurement semantics.
- Required platform functions (serial, time, sleep, allocation where injected,
  bridge transport) are narrow backend dependencies, not `GlobalState` fields.

## Current implementation boundary

- **Conforming direction:** `asic_driver.c` centralizes model selection;
  `asic_capabilities.c` lets Stratum choose rolling/refresh by capability; common
  work/result types and health snapshots exist.
- **Violations:** Public `asic.h` includes `global_state.h`; operations take
  `GlobalState`; the component adds private `main/`/`main/tasks` include paths
  and requires Stratum components; model headers include Stratum `mining.h`;
  generic initialization does not validate a complete mandatory operation set.
- **Target seam:** Opaque instance plus immutable descriptor and injected ports;
  compatibility wrappers may translate `GlobalState` only at composition until
  consumers migrate.

## Required and forbidden behavior

- Backend-private details stay in backend translation units/headers not exposed
  to generic features.
- A driver must not parse Stratum, submit shares, decide self-test outcomes,
  persist settings, render status, or own board regulator policy.
- A feature must not infer capability from ASIC name, board number, NULL callback,
  or a runtime error that could represent hardware failure.
- Construction/start fails before mining tasks when mandatory state is absent.

## Relationships to other feature slices

| Related feature | Relationship |
|---|---|
| [Feature ownership and dependency direction](../../architecture/feature-ownership/SPEC.md) | Defines the ports/adapters and composition rules this facade follows. |
| [Protocol-neutral mining work pipeline](../../mining/work-pipeline/SPEC.md) | Supplies backend-independent work, handles, results, and events. |
| [Bitmain ASIC backend](../bitmain-work-pipeline/SPEC.md) | Implements this contract for BM1397/BM1366/BM1368/BM1370. |
| [Factory self-test lifecycle](../../manufacturing/factory-self-test/SPEC.md) | Consumes diagnostic lifecycle and canonical snapshots through this contract. |
| [Thermal, fan, and overheat control](../../device-safety/thermal-fan-overheat/SPEC.md) | Owns safety policy and supplies approved operating requests. |

## Verification approach

- Compile public contracts and a fake driver on the host, test lifecycle and
  failure matrices, then run backend unit/QEMU and authorized board qualification.
- **Exact revision:** `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f`
- **Evidence classes:** source/test-surface reconciliation only.

## Constraint provenance

- **Current repository evidence:** Driver/capability dispatch is present, while
  public application-state and upward dependency leaks are directly visible.
- **Historical review evidence:** Merged PR #747 established callback-based
  model isolation and one ASIC type per board; current architecture analysis and
  the affected specs extend that accepted direction to lifecycle/diagnostics.
- **Disposition:** Target contract is implementing; existing driver dispatch is
  evidence of direction, not proof of full isolation.
