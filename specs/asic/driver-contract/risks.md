# ASIC driver contract — risks and scope

## Scope

### In

- Common ASIC construction, descriptor/capabilities, lifecycle, work/events,
  diagnostics, telemetry/health, failure, backend isolation, and evidence.

### Out

- Pool protocol, self-test pass/fail policy, board qualification values, power
  safety policy, frontend behavior, and concrete BM/BZM implementation details.

### Review unit

- **Why this is the smallest coherent change:** Public context, lifecycle,
  descriptor, and result ownership must agree before individual consumers move.
- **Work intentionally split out:** Backend adapters and each consumer migrate
  in separate compatible steps.

## Assumptions

- Both Bitmain and BZM behavior can be represented without exposing their raw
  topology/protocol to feature services.

## Open questions

- Which operations are mandatory for every backend, and which are explicit
  optional capability groups?
- Which low-level dependencies should be injected versus kept inside an
  ESP-specific backend adapter?

## Failure modes

| Failure | Impact | Detection | Mitigation or recovery |
|---|---|---|---|
| Contract mirrors Bitmain | BZM exceptions leak upward | Fake/BZM implementation review | Define consumer needs and neutral semantics |
| Capability inferred from NULL op | Unsupported confused with fault | Contract matrix test | Explicit capability and typed status |
| Stop semantics differ silently | Unsafe restart/teardown | Lifecycle failure injection/HIL | Reason-aware idempotent stop contract |
| Descriptor owns mutable runtime | Races and shadow state | API/ownership review | Immutable descriptor; separate snapshot |
| Compatibility wrapper becomes permanent | `GlobalState` coupling remains | Dependency check | Removal criterion for every wrapper |

## Ownership and boundary risks

- **Shadow or duplicated state:** Lifecycle status must replace, not accompany
  indefinitely, `ASIC_initalized` and backend-local claims.
- **Allocation, pointer, callback, or task lifetime:** Opaque instance, event,
  and snapshot ownership must be explicit and instance-safe.
- **Untrusted input and copy/parse bounds:** UART/bridge input remains backend
  untrusted input and must be normalized only after validation.
- **Blocking, race, lock, queue, timeout, or reconnect behavior:** Driver calls
  declare blocking limits and serialized/concurrent safety.
- **Cross-layer drift:** Descriptor/API projections require schema and frontend
  reconciliation without exposing backend-only fields.

## Security, privacy, and safety

- A generic facade cannot weaken backend safe-off, bridge lease, reset, thermal,
  or regulator preconditions. Runtime failure remains distinguishable.

## Performance and resource risks

- Per-instance contexts and owned snapshots add bounded memory; measure static
  DRAM, PSRAM, task stacks, work latency, and event throughput.

## Rollout and rollback

- Keep the current facade as a temporary adapter until Bitmain and BZM pass the
  same contract suite; rollback selects the old wrapper at composition.
