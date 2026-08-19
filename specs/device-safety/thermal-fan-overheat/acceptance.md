# Thermal, fan, and overheat control — acceptance

## Functional behavior

- [x] **SAFE-THERMAL-AC-01:** Fan output is clamped to the board floor and 100%,
  and a failed fan write records a hardware fault.
- [x] **SAFE-THERMAL-AC-02:** Automatic PID waits for a valid positive ASIC
  temperature and uses the hotter available sensor; startup uses a safe fallback.
- [x] **SAFE-THERMAL-AC-03:** Generic frequency changes call the active ASIC
  driver's frequency callback rather than switching on ASIC model.
- [x] **SAFE-THERMAL-AC-04:** Stop/start reuse ASIC reset and initialization
  paths, validate power before init, and clear stale UART data at the lifecycle
  boundary.
- [ ] **SAFE-THERMAL-AC-05:** Overheat evaluation outranks user pause, hardware
  fault, and pool-unavailable convenience paths. Current generic power task can
  enter the pause branch and `continue` before overheat evaluation.
- [ ] **SAFE-THERMAL-AC-06:** Thermal/device configuration exposes sensor
  topology and readings through a model-independent collection/capability
  interface. Current consumers still use fixed `temp`/`temp2` paths.
- [ ] **SAFE-THERMAL-AC-07:** Focused tests cover invalid/missing sensors, two
  sensors, fan floors/clamps/write failure, priority ordering, cooling proof,
  failed restart, serial reset, and board-specific offsets.
- [ ] **SAFE-THERMAL-AC-08:** Generic fan, thermal, and power policy compiles
  without `CONFIG_BZM_*`, `bonanza_bridge`, concrete board/driver headers, or
  `GlobalState`; composition injects ports and an immutable safety profile.
- [ ] **SAFE-THERMAL-AC-09:** Bitmain and BZM adapters implement the same
  thermal/power/reset contracts, and feature policy does not use a
  `bonanza`/board Boolean to choose behavior.

## Verification evidence

| Evidence class | Exact command, case, revision, target, result, and date |
|---|---|
| Source reconciliation | Inspected fan, power management, thermal, device configuration, ASIC callback/reset code at `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f` — AC-01 through AC-04 present; AC-05 and AC-06 gaps found, 2026-08-10 |
| Focused unit evidence | Existing `components/asic/test/test_bzm_overheat_recovery.c` covers Bonanza cooling gates/reduced targets; not run in this documentation change. |
| Firmware build/QEMU | Not run; no implementation changed. |
| Authorized mining/HIL | Not run; physical hardware validation was not authorized. |

## Acceptance rule

Safety behavior is not complete on source inspection alone: affected board
classes require automated boundary evidence and authorized relevant-device
validation when the control path changes.
