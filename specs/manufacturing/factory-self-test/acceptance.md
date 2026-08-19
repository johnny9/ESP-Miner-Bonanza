# Factory self-test lifecycle — acceptance

## Functional and lifecycle behavior

- [x] **MFG-SELFTEST-AC-01:** Semaphore creation is checked before the reset path
  can give/take it, and reset safely ignores an uninitialized semaphore.
- [ ] **MFG-SELFTEST-AC-02:** Every message/finished string retained in global
  state is copied into self-test-owned storage. Current `message` and successful
  countdown `finished` can point to stack buffers.
- [ ] **MFG-SELFTEST-AC-03:** After any initialization or prerequisite failure,
  control flow explicitly returns or transitions once to a non-returning terminal
  handler; no later test step can execute accidentally.
- [x] **MFG-SELFTEST-AC-04:** Dynamic domain-average allocation is checked and
  freed on the normal completion path.
- [x] **MFG-SELFTEST-AC-05:** Terminal handling stops nonce measurement, sets
  VCORE to zero, and holds the ASIC in reset before restart/wait.
- [ ] **MFG-SELFTEST-AC-06:** Focused tests inject semaphore/allocation/init
  failure, exercise stack-message lifetime, invalid sensors, fan/power/domain
  failures, pass/fail cleanup, NVS flag behavior, and reset/restart transitions.
- [ ] **MFG-SELFTEST-AC-07:** Self-test policy compiles without `GlobalState`,
  Stratum headers, concrete ASIC/board/driver headers, or ESP-IDF/NVS/GPIO APIs;
  platform integration is supplied only by typed ports at composition.
- [ ] **MFG-SELFTEST-AC-08:** Hashrate diagnostics submit deterministic neutral
  work through the mining pipeline with a self-test purpose and consume only
  results carrying the matching work identity; no Stratum JSON or queue is used.
- [ ] **MFG-SELFTEST-AC-09:** An immutable board qualification profile declares
  applicable checks, safe thresholds, and required capabilities. Unsupported
  checks are reported as not applicable, never silently passed.

## Verification evidence

| Evidence class | Exact command, case, revision, target, result, and date |
|---|---|
| Source reconciliation | Inspected `main/self_test/self_test.c` and global self-test state at `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f` — AC-01, AC-04, AC-05 present; AC-02/AC-03 gaps found, 2026-08-10 |
| Focused regression | Not run; documentation-only reconciliation. AC-06 remains unchecked. |
| Firmware build/QEMU | Not run; no implementation changed. |
| Factory-board/HIL | Not run; no physical test device was authorized. |

## Known current contradictions

- `main/self_test/self_test.c` directly calls DS4432U, thermal, VCORE, power,
  reset, GPIO, NVS, PID, ASIC, and monitoring implementations through
  `GlobalState` and `DeviceConfig`.
- It mutates ASIC difficulty/version state, parses hard-coded Stratum JSON, and
  enqueues that protocol object to generate diagnostic work. This violates the
  self-test, Stratum, and mining-work ownership boundaries.

## Acceptance rule

Changes to self-test require failure-injection evidence and, when hardware
thresholds or actuation change, explicit authorized validation on each affected
board family with recovery available.
