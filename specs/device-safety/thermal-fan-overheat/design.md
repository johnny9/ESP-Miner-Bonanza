# Thermal, fan, and overheat control — design

## Components and responsibilities

| Component | Responsibility | Implementation pointer |
|---|---|---|
| Safety profile | Immutable sensor topology, offsets, fan floor, and voltage/frequency limits | Target profile composed from `main/device_config.c` |
| Thermal/power/reset ports | Portable readings and safe actuator/lifecycle operations | Target interfaces over `main/thermal/thermal.c` and `main/power/` |
| Concrete hardware adapters | Implement ports for Bitmain boards and BZM bridge boards | Current implementation mixed into `main/thermal/thermal.c`, `main/power/`, and `main/bzm_controller.c` |
| Fan task | Mode priority, PID, clamp, diagnostics, failure flag | `main/tasks/fan_controller_task.c` |
| Power task/controller | Overheat priority and shared mining stop/start/recovery | `main/tasks/power_management_task.c`, `main/bzm_controller.c` |
| ASIC abstraction | Purpose-aware start, reason-aware stop, diagnostics, and frequency capability | [ASIC driver contract](../../asic/driver-contract/SPEC.md) |

## Authority and dependency direction

| Fact or state | Owner | Consumers | Forbidden duplicate |
|---|---|---|---|
| Sensor presence/count/offset | Immutable safety profile and thermal port | Fan/power/API | Board-model conditionals in generic task |
| Fan minimum and actuator | Thermal capability API | Settings validator and fan task | UI or task-local magic floor |
| Overheat/recovery state | Safety controller/power task | Fan task, API, UI | User pause masking safety entry |
| Frequency operation | Active ASIC driver callback | Power lifecycle | ASIC switch in generic transition code |
| Voltage operation | Power/VCORE layer | Power lifecycle | ASIC driver owning board regulator policy |

## Interfaces and cross-layer matrix

| Surface | Required reconciliation |
|---|---|
| Firmware | Sensor capability, priority, stop/start, failure and recovery |
| OpenAPI | Temperatures, fan state, overheat status, units and missing values |
| AxeOS | Display state and constrained controls without re-derived limits |
| NVS/settings | Safe ranges; safety state cannot be enabled by public PATCH |
| Tests | Pure priority/PID/cooling cases plus driver failure injection |
| HIL | Affected board sensors, fan response, safe-off, restart, mining soak |

## Required and forbidden behavior

- Priority is hardware safe-off/fan control, overheat, other hardware fault,
  user pause/pool loss, automatic PID, then manual fan control.
- Ignore invalid sentinel/non-finite temperatures for PID; missing safety
  telemetry fails closed where required by the board.
- Clamp every actuator command and log enough temperature/fan RPM context to
  diagnose transitions.
- Reuse one ASIC stop/start/reset/serial path; do not duplicate restart logic.
- Do not expose unused generic PID modes/directions as public controls.
- Concrete board/bridge/ESP adapters may use configuration macros and driver
  APIs; portable safety policy may not.

## Failure, ownership, and lifecycle boundaries

- Power management owns the transition from mining to stopped and back.
- The ASIC remains reset/power-safe while restart prerequisites are unproven.
- Tasks observe shared safety state but do not shadow initialization flags when
  an authoritative driver/state query exists.
- Cooling waits are bounded and periodically refresh the sensors that remain
  valid while the ASIC is powered down.

## Compatibility and resources

- Older boards with one sensor and newer boards with multiple sensors use the
  same collection/capability contract.
- Control loops keep fixed cadence and avoid blocking unrelated event loops.
- NVS writes on overheat/recovery are minimized and ordered after safety action.

## Current implementation boundary

- **Conforming:** shared frequency callbacks and several common stop/start paths
  reduce model-specific logic in generic tasks.
- **Violations:** `main/thermal/thermal.c`, `main/power/vcore.c`, and
  `main/power/asic_reset.c` directly branch into BZM/bridge implementations;
  `main/tasks/fan_controller_task.c` contains `CONFIG_BZM_*` and direct Bonanza
  bridge calls; generic paths consume `DeviceConfig`/`GlobalState` directly.
- **Target seam:** portable safety policy consumes a safety profile and injected
  thermal/fan/power/reset/ASIC ports. Board-specific adapters are selected once
  at composition and own all bridge, regulator, GPIO, and ESP details.

## Relationships to other feature slices

| Related feature | Relationship |
|---|---|
| [Bitmain ASIC backend](../../asic/bitmain-work-pipeline/SPEC.md) | Implements model-specific frequency mechanics while this safety slice retains voltage, thermal priority, and safe-stop authority. |
| [Settings validation and persistence](../../configuration/settings-persistence/SPEC.md) | Validates operator requests against safety-owned limits. |
| [Factory self-test lifecycle](../../manufacturing/factory-self-test/SPEC.md) | Temporarily controls fan/power while preserving safety and cleanup. |
| [Feature ownership and dependency boundaries](../../architecture/feature-ownership/SPEC.md) | Defines portable policy, hardware ports, concrete adapters, and composition. |
| [ASIC driver contract](../../asic/driver-contract/SPEC.md) | Supplies a backend-neutral lifecycle rather than concrete Bitmain/BZM calls. |

## Verification approach

- Isolate priority and recovery decisions for deterministic unit tests, then run
  firmware/QEMU and explicitly authorized affected-board thermal/mining tests.
- **Exact revision:** `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f`
- **Evidence classes:** source reconciliation and existing unexecuted unit-test
  pointers only in this change.

## Constraint provenance

- **Current repository evidence:** Clamp, invalid-temperature guard, driver
  callback, and shared init/reset paths exist; priority and generic sensor
  topology remain incomplete.
- **Historical review evidence:** [PR #800](https://github.com/bitaxeorg/ESP-Miner/pull/800) (WantClue/mutatrum, merged; invalid startup temp, clamp, RPM diagnostics, no unused PID controls), [PR #1172](https://github.com/bitaxeorg/ESP-Miner/pull/1172) and [PR #962](https://github.com/bitaxeorg/ESP-Miner/pull/962) (mutatrum, merged; thermal/device-config ownership), [PR #747](https://github.com/bitaxeorg/ESP-Miner/pull/747) (mutatrum, merged; callbacks and power/ASIC boundary), [PR #1608](https://github.com/bitaxeorg/ESP-Miner/pull/1608) (WantClue/mutatrum, merged; 13/13 relevant threads resolved; power guardian and overheat priority), and [PR #1304](https://github.com/bitaxeorg/ESP-Miner/pull/1304) (WantClue/mutatrum, merged; shared restart/reset/serial path).
- **Disposition:** Accepted and mostly corroborated; current gaps are explicit.
