# Protocol-neutral mining work pipeline — risks and scope

## Scope

### In

- Neutral work/result types, typed queueing, source generation/origin, work
  provenance, candidate validation/routing, diagnostic purpose, and observers.

### Out

- Protocol parsing/session policy, ASIC packet/register mechanics, self-test
  pass/fail policy, and observer presentation/storage implementation.

### Review unit

- **Why this is the smallest coherent change:** Work ownership and result
  provenance must share one identity/generation contract.
- **Work intentionally split out:** Moving each source, backend, observer, and
  task occurs in compatible follow-up changes.

## Assumptions

- Protocol-specific submission data can remain behind a source-owned opaque
  origin handle for the lifetime of a session generation.

## Open questions

- Should observer delivery be synchronous bounded callbacks or a typed event
  queue with fixed fan-out/backpressure policy?
- How should hardware slot reuse be fenced for backends lacking generation bits?

## Failure modes

| Failure | Impact | Detection | Mitigation or recovery |
|---|---|---|---|
| Origin outlives session | Wrong-session share/use-after-free | Close/result race test | Generation-scoped source-owned route |
| Slot aliases new work | Stale share attribution | Delayed result/slot-wrap test | Pipeline generation plus backend flush/reuse rule |
| Observer blocks result path | Lost throughput/watchdog | Slow observer test | Bounded delivery or decoupled event queue |
| Diagnostic work reaches pool | Invalid share/protocol coupling | Self-test integration test | Purpose-specific no-share sink |
| Compatibility path double-accounts | Duplicate stats/submission | Dual-path regression | One authority and staged cutover |

## Ownership and boundary risks

- **Shadow or duplicated state:** Source generation, clean epoch, work handle,
  and current protocol must not remain parallel authorities.
- **Allocation, pointer, callback, or task lifetime:** Work/origin/event clone,
  free, and source-close rules are central.
- **Untrusted input and copy/parse bounds:** Protocol adapters validate before
  publication; ASIC adapters validate raw results before normalization.
- **Blocking, race, lock, queue, timeout, or reconnect behavior:** Queue and
  source-close/result races require deterministic tests.
- **Cross-layer drift:** Observer payload/API changes reconcile schema, UI, and
  persistence without adding those concerns to the pipeline.

## Security, privacy, and safety

- Opaque origins must not carry loggable pool credentials. Invalid work/results
  fail closed and cannot bypass ASIC safe-state prerequisites.

## Performance and resource risks

- Owned templates/provenance/events add bounded memory and copies; measure heap,
  fragmentation, queue depth, result latency, and share throughput.

## Rollout and rollback

- Adapt one producer/consumer at a time behind compatibility ports; rollback
  restores the prior wiring without changing NVS or pool configuration.
