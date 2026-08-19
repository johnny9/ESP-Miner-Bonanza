# Protocol-neutral mining work pipeline — acceptance

## Work and result behavior

- [x] **MINE-WORK-AC-01:** Current code has owned `mining_template_t` cloning,
  normalized `asic_result_t`/`asic_event_t`, a bounded job store, and common
  result validation/routing helpers.
- [ ] **MINE-WORK-AC-02:** Portable mining contracts live outside Stratum and
  ASIC backend ownership and include no ESP-IDF, concrete protocol, backend, or
  application-state types.
- [ ] **MINE-WORK-AC-03:** Each queued work envelope carries immutable kind,
  purpose, source generation, opaque origin, destructor, and ownership.
- [ ] **MINE-WORK-AC-04:** Work-template derivation is implemented by source
  adapters before publication or by a neutral factory; the mining consumer does
  not cast V1/V2 payloads based on mutable global state.
- [ ] **MINE-WORK-AC-05:** The pipeline owns hardware-handle↔work provenance and
  stale-generation rejection; ASIC backends own only private hardware slots.
- [ ] **MINE-WORK-AC-06:** Result processing emits a validated share candidate
  to an opaque originating share sink and immutable typed events to independent
  accounting, scoreboard, monitoring, and diagnostic observers.
- [ ] **MINE-WORK-AC-07:** Self-test creates deterministic diagnostic work and
  receives purpose-tagged results without Stratum parsing, queues, flags, or
  protocol submission metadata.
- [ ] **MINE-WORK-AC-08:** Allocation, queue, send, stale work, source close,
  result fan-out, observer failure, and diagnostic cases have host/unit tests;
  applicable QEMU/mining/HIL evidence remains distinct.

## Verification evidence

| Evidence class | Exact command, case, revision, target, result, and date |
|---|---|
| Source reconciliation | Partial neutral types/ownership exist at `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f`; location, queue, routing, and self-test dependencies contradict AC-02 through AC-07, 2026-08-11. |
| Existing tests | `components/asic/test/test_bm_job.c` covers cloning, generated handles, routing, and stale work; not run in this documentation change. |
| Host/QEMU | No independent portable pipeline suite identified or run. |
| Mining/HIL | Not run; no device or pool operation was authorized. |

## Acceptance rule

The pipeline is complete only when two protocol sources, a fake ASIC, a
diagnostic source, and multiple observers can use it without including each
other's implementation or mutable application state.
