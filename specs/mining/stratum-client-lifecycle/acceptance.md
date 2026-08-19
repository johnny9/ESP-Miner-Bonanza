# Stratum client lifecycle and pool failover — acceptance

## Client behavior and state

- [x] **MINE-CLIENT-AC-01:** The current client supports a primary `pool` and an
  optional `fallback pool`, each with its own V1/V2 protocol selection.
- [x] **MINE-CLIENT-AC-02:** Automatic fallback probes primary and returns when
  it recovers, while an explicitly selected fallback remains sticky.
- [x] **MINE-CLIENT-AC-03:** Exhausting all configured pools sets an observable
  unavailable state used to pause ASIC power, then bounded probes can resume.
- [ ] **MINE-CLIENT-AC-04:** Coordinator queue allocation and every protocol-task
  creation return success/failure; failure cannot leave state marked running.
- [ ] **MINE-CLIENT-AC-05:** Every queued work item carries immutable payload
  type, destructor, source session/epoch, and ownership independent of mutable
  global protocol/channel state.
- [ ] **MINE-CLIENT-AC-06:** A stopped V1/V2 task releases transport, Noise,
  parser, connection, and queue resources exactly once before replacement.
- [ ] **MINE-CLIENT-AC-07:** Difficulty, version mask, extranonce, active pool,
  and transport publication are transferred as coherent session state; mining
  tasks cannot observe a flag with stale companion data or use a retired handle.
- [ ] **MINE-CLIENT-AC-08:** Failure injection and reconnect regressions cover
  queue/task allocation failure, stop timeout, V1↔V2 and standard↔extended
  transitions, user-selected fallback, all-pools-down recovery, and repeated
  reconnects without heap growth.
- [ ] **MINE-CLIENT-AC-09:** Stratum client/session policy has no dependency on
  `asic.h`, `device_config.h`, concrete board/driver headers, or `GlobalState`.
  Required rolling and channel capabilities arrive as an immutable mining
  capability snapshot.
- [ ] **MINE-CLIENT-AC-10:** The client exposes only neutral work-source and
  share-sink ports to the mining pipeline. It neither sends ASIC work nor polls,
  validates, scores, displays, or routes ASIC results.

## Verification evidence

| Evidence class | Exact command, case, revision, target, result, and date |
|---|---|
| Source reconciliation | Inspected coordinator, V1/V2 tasks, work queue, mining work/result tasks, system cleanup, and power-unavailable state at `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f` — AC-01 through AC-03 present; AC-04 through AC-07 have the gaps recorded below, 2026-08-10 |
| Existing test surface | Protocol component tests exist, but no complete coordinator/failover lifecycle suite was identified or run in this documentation change. |
| Firmware/QEMU | Not run; no implementation changed. |
| Mining/soak/HIL | Not run; no pool/device operation was authorized. |

## Known current contradictions

- `protocol_coordinator_init()` does not report `xQueueCreate()` failure, and
  protocol task creation logs failure while coordinator state remains running.
- `work_queue` stores untyped pointers with one mutable queue-wide destructor;
  `create_jobs_task` explicitly falls back to `free()` during a protocol race,
  which can leak V1-owned members.
- V1 normal close detaches and closes its transport but does not destroy it;
  repeated reconnect cleanup is therefore not yet complete at this revision.
- Difficulty is now a `double` plus a separate unsynchronized notification flag;
  the older review conclusion that a `uint32_t` write was atomic does not by
  itself establish a coherent current handoff.
- V1/V2 tasks include the ASIC facade, V2 includes device configuration, and
  `create_jobs_task`/`asic_result_task` combine protocol, ASIC, monitoring, UI,
  and scoreboard ownership.

## Acceptance rule

Client lifecycle changes require deterministic state-transition tests plus a
reconnect/failover heap-soak result on the affected target. Logging an error is
not sufficient when the state machine continues as though startup succeeded.
