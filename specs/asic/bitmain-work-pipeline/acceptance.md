# Bitmain ASIC backend — acceptance

## Work, driver, and result behavior

- [x] **ASIC-BITMAIN-AC-01:** BM1397, BM1366, BM1368, and BM1370 are selected
  through one driver table, and generic Stratum/work/result tasks do not switch
  on Bitmain model.
- [x] **ASIC-BITMAIN-AC-02:** The common Bitmain builder consumes validated
  binary header/work fields and produces deterministic packet
  material/midstates without parsing V1 or V2.
- [x] **ASIC-BITMAIN-AC-03:** Each sent hardware slot stores a mutex-protected
  clone of submission metadata; unknown/invalidated work is rejected.
- [x] **ASIC-BITMAIN-AC-04:** Raw Bitmain share/register responses become
  distinct normalized events; register events never become share candidates.
- [x] **ASIC-BITMAIN-AC-05:** Version rolling and work refresh policy are
  capabilities consumed by Stratum/mining code, not chip-name conditions there.
- [x] **ASIC-BITMAIN-AC-06:** Common frequency transition receives the selected
  driver's frequency callback; voltage and safety policy stay in power control.
- [ ] **ASIC-BITMAIN-AC-07:** Job-store, required-driver-operation, serial/task,
  and other prerequisite initialization failures prevent the mining stack from
  starting and publish a precise degraded/fault state.
- [ ] **ASIC-BITMAIN-AC-08:** Focused tests cover every model's packet/result
  layout, chip-ID/CRC and count detection, reset/serial ordering, PLL constraints,
  clean-job/slot reuse, malformed UART data, and single/multi-chip operation.
- [ ] **ASIC-BITMAIN-AC-09:** Authorized hardware evidence establishes safe
  model-specific init, official frequency bounds, transition behavior, work
  cadence, no duplicate-search regression, and accepted shares.
- [ ] **ASIC-BITMAIN-AC-10:** Public ASIC driver headers expose opaque device
  context and neutral values only; they do not include `GlobalState`,
  `DeviceConfig`, Stratum-owned mining types, or concrete Bitmain headers.
- [ ] **ASIC-BITMAIN-AC-11:** `components/asic` has no build dependency on
  `stratum`, `stratum_v2`, or private include path into `main/`. Bitmain hardware
  slots and packet details remain backend-private.

## Verification evidence

| Evidence class | Exact command, case, revision, target, result, and date |
|---|---|
| Source reconciliation | Inspected ASIC driver/capabilities, neutral templates, Bitmain builder/results, model drivers, job store, result handler, mining tasks, and system initialization at `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f` — AC-01 through AC-06 present; AC-07 remains contradicted, 2026-08-10 |
| Existing focused tests | `components/asic/test/test_bm_job.c`, `test_capabilities.c`, `test_job_command.c`, and `test_timeout.c` provide partial coverage; not run in this documentation change. |
| Firmware/QEMU | Not run; no implementation changed. |
| Mining/soak/HIL | Not run; no pool/device operation was authorized. AC-08 and AC-09 remain unchecked. |

## Known current contradictions

- `SYSTEM_init()` logs `asic_job_store_init()` failure but continues, while all
  Bitmain send/result paths assume the store mutex is initialized.
- Generic driver tests verify core operations, but runtime initialization does
  not validate the complete required operation set before declaring the chain
  usable.
- Bitmain results contain small hardware slot IDs rather than generation-bearing
  handles; clean-job invalidation and slot-reuse behavior therefore need explicit
  stale-result tests and a documented safe reuse rule.
- `components/asic/CMakeLists.txt` currently requires both Stratum components
  and adds private include paths into `main`/`main/tasks`; public ASIC headers
  expose `GlobalState`, `DeviceConfig`, and Stratum-owned mining types.

## Acceptance rule

Pure builders and result routing require deterministic unit vectors. Reset,
serial, chip-count, PLL/frequency, timing, and accepted-share claims require the
specific ASIC/board target; do not infer them from QEMU or source inspection.
