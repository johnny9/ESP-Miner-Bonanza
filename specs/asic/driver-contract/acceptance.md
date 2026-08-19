# ASIC driver contract — acceptance

## Driver behavior and isolation

- [x] **ASIC-DRIVER-AC-01:** Current model selection uses a driver table and
  capability data instead of model switches in Stratum work cadence.
- [ ] **ASIC-DRIVER-AC-02:** Public contract operations use an opaque ASIC
  instance context and portable types; no public ASIC header includes
  `GlobalState`, `DeviceConfig`, Stratum codecs, or concrete backend headers.
- [ ] **ASIC-DRIVER-AC-03:** Construction validates mandatory operations and an
  immutable descriptor before any hardware side effect.
- [ ] **ASIC-DRIVER-AC-04:** `prepare`, purpose-aware `start`, reason-aware
  `stop`, status, and teardown have explicit idempotence and safe-state rules.
- [ ] **ASIC-DRIVER-AC-05:** Mining work/results use the neutral mining pipeline;
  backend hardware slots and raw packets remain private.
- [ ] **ASIC-DRIVER-AC-06:** Self-test consumes descriptor/profile facts,
  diagnostic start/stop, and canonical snapshots rather than Bitmain topology,
  Stratum work, or backend globals.
- [ ] **ASIC-DRIVER-AC-07:** Bitmain and BZM report common validity-aware health
  and telemetry while preserving explicit optional capabilities.
- [ ] **ASIC-DRIVER-AC-08:** The ASIC component has no private `main/` include
  path or upward Stratum/application dependency; dependency checks enforce it.
- [ ] **ASIC-DRIVER-AC-09:** Contract, fake-backend, Bitmain, BZM, failure,
  lifecycle, and applicable authorized hardware tests establish parity.

## Verification evidence

| Evidence class | Exact command, case, revision, target, result, and date |
|---|---|
| Source reconciliation | Driver table/capabilities exist at `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f`; public context and component dependencies contradict AC-02 through AC-08, 2026-08-11. |
| Existing tests | Driver/capability and backend tests exist under `components/asic/test/`; not run in this documentation change. |
| Host/QEMU | No portable fake-driver contract suite identified or run. |
| HIL | Not run; no device operation was authorized. |

## Acceptance rule

A backend is conforming only when the common contract and its target-specific
safety/mining evidence pass. A NULL operation or board branch in a consumer is
not an acceptable substitute for explicit capability/support status.
