# Stratum client lifecycle and pool failover — risks and scope

## Scope

### In

- Pool/protocol selection, task start/stop, failover/pause/recovery, active
  session publication, typed work ownership, and share-transport lifetime.

### Out

- V1/V2 parser internals, ASIC packet formats, pool economics, and n>2 product
  support.

### Review unit

- **Why this is the smallest coherent change:** Coordinator state, task
  lifetime, and work ownership form one transition boundary; changing one
  without reconciling the others can create stale work or overlapping sessions.
- **Work intentionally split out:** Protocol parsing and ASIC-family encoding
  remain separately reviewable features.

## Assumptions

- One protocol task is active at a time and the current product exposes at most
  a primary pool plus one fallback.
- The target can interrupt a blocked receive by closing the underlying transport.

## Open questions

- Should current session state become one immutable snapshot or a sequence of
  typed events? Either must define transport/reference lifetime explicitly.
- What target-specific stop timeout is safe before entering a non-recoverable
  degraded state?

## Failure modes

| Failure | Impact | Detection | Mitigation or recovery |
|---|---|---|---|
| Task creation fails after state changes | Phantom running client | Inject task failure and inspect state/task count | Start returns failure; transition to failover/pause |
| Queue item type follows mutable protocol | Invalid free/cast or leak | Protocol switch during blocked dequeue | Immutable per-item type/destructor/source |
| Retired transport used for share | Use-after-free or wrong-pool submit | Concurrent stop/submit test | Protected session snapshot/reference or serialized submit |
| V1 close omits destroy | Reconnect heap loss | Repeated-failure heap soak | Single owner closes and destroys exactly once |
| Late task event | New session incorrectly failed | Event generation mismatch | Tag and reject retired-session events |
| All pools unavailable | Hot idle ASIC wastes power | Coordinator state plus power telemetry | Pause power and bounded recovery probes |

## Ownership and boundary risks

- **Shadow or duplicated state:** Active protocol, fallback flag, username,
  difficulty, extranonce, and transport currently live in separate fields.
- **Allocation, pointer, callback, or task lifetime:** Queue payloads, transport,
  Noise, parser buffers, task handles, and event queue each require one owner.
- **Untrusted input and copy/parse bounds:** Delegated to the session-safety
  spec; client must not publish work when parsing/decoding fails.
- **Blocking, race, lock, queue, timeout, or reconnect behavior:** Highest risk;
  every blocking operation and cross-task snapshot needs a finite boundary.
- **Cross-layer drift:** API status must reflect coordinator truth, not infer
  active pool/protocol in AxeOS.

## Security, privacy, and safety

- Never log pool passwords, certificates, private keys, or full authorization
  material. A failed client must stop unsafe or wasteful ASIC operation.

## Performance and resource risks

- Heartbeat/recovery probes occupy the coordinator for bounded network waits;
  queue envelopes and snapshots add memory but remove ambiguous ownership.

## Rollout and rollback

- Preserve NVS and API representation. Rollback may reboot into the prior client,
  but it must first close the active session or rely on reboot to release it.
