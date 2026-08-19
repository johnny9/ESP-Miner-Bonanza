# Stratum client lifecycle and pool failover — design

## Components and responsibilities

| Component | Responsibility | Implementation pointer |
|---|---|---|
| Pool configuration | Persist two public pool slots and their protocol settings | `main/system.c`, `main/system.h`, `main/nvs_config.c`, `main/nvs_config.h` |
| Protocol coordinator | Select pool/protocol, process lifecycle events, fail over, pause, probe, and resume | `main/tasks/protocol_coordinator.c`, `main/tasks/protocol_coordinator.h` |
| Protocol task | Own one V1 or V2 session from allocation through final cleanup | `main/tasks/stratum_v1_task.c`, `main/tasks/stratum_v2_task.c` |
| Work-source adapter | Convert validated V1/V2 work into a neutral envelope for the mining service | Target extraction from `main/work_queue.c`, `main/work_queue.h`, and `main/tasks/create_jobs_task.c` |
| Share-sink adapter | Submit a candidate only through its retained source-session identity | Target extraction from `main/tasks/asic_result_task.c` into V1/V2 session adapters |
| System/power state | Clear queued/ASIC work and stop power when pools are unavailable | `main/system.c`, `main/tasks/power_management_task.c` |

## Authority and dependency direction

| Fact or state | Owner | Consumers | Forbidden duplicate |
|---|---|---|---|
| Persisted pool/fallback settings | NVS/system configuration | Coordinator and protocol task | Task-local alternate pool configuration |
| Active pool, protocol, and session generation | Protocol coordinator | Session, work handoff, API/status | Inferring source from mutable payload shape or username |
| Transport/Noise/parser resources | Active protocol task | Protocol I/O and synchronized share submit | Coordinator destroying task-owned resources after replacement starts |
| Queued work type/destructor/source | Mining work envelope | Mining service | Queue-wide mutable destructor or post-dequeue protocol guess |
| Session mining parameters | Active session state/message | Work-template builder | Separate flag/data writes without a coherent publication rule |

## Interfaces and contracts

### Pool and protocol contract

- Public naming is `pool` and `fallback pool`, matching AxeOS and API language.
- The supported public shape is currently two slots. A pool settings struct or
  future array may reduce duplication, but n>2 is not an accepted requirement.
- A user-selected fallback is policy, not a failure state; automatic fallback
  alone enables return-to-primary probing.

### Lifecycle event contract

- Coordinator initialization and session start return a checked result.
- A task may report `setup succeeded`, `failed`, or `exited after stop`; events
  include enough session identity to reject late events from a retired task.
- State is changed to running only after required allocation/task creation
  succeeds. Protocol readiness is published only after complete setup.

### Work handoff contract

- A work envelope owns its payload and contains an immutable kind, destructor,
  source session generation, protocol/channel kind, and clean-work generation.
- Queue capacity replacement and clear happen under the queue lock and call the
  stored per-item destructor exactly once.
- The consumer builds from the envelope type; it never casts based on the
  current global protocol or channel.
- Stratum is a work source and share sink. The mining service owns dispatch,
  ASIC work handles, provenance, result validation, and observers.

## Required and forbidden behavior

- Finite probes may block the coordinator for their documented connect/read
  limits; do not describe them as non-blocking.
- Pool/session transitions clear queued work and invalidate ASIC job metadata
  before work from the new session can be accepted.
- Do not start a replacement task until the previous task has relinquished its
  session resources or the system has entered a contained failed state.
- Do not let protocol-specific parsing, Noise, or packet construction accumulate
  in the coordinator; expose only coordinator functions used outside its file.
- Do not submit a share through a transport handle whose lifetime is no longer
  protected after the active-session snapshot is taken.

## Failure, ownership, and concurrency boundaries

- Queue/event/task allocation failure follows the same failover/pause contract
  as protocol setup failure and remains visible in status.
- Closing the transport may unblock a receive, but the protocol task remains the
  final owner that destroys transport and protocol resources exactly once.
- Session state can be transferred by a locked snapshot, immutable message, or
  equivalent atomic design; a data field plus an unrelated Boolean is not a
  sufficient contract when the data is wider than target atomic guarantees.
- Stop waits are finite. Timeout prevents overlapping ownership and exposes a
  degraded/failed state rather than silently continuing.

## Compatibility and resources

- Preserve existing NVS/API names and the V1/V2 and standard/extended public
  values. Pool structure refactoring requires a migration only if storage or API
  representation changes.
- Queue depth, task stacks, PSRAM requirements, probe cadence, retry limits, and
  shutdown limits are bounded resource policy and require target evidence when
  changed.

## Current implementation boundary

- **Conforming:** pool/fallback policy and V1/V2 session selection are centered
  in the protocol coordinator.
- **Violations:** V1/V2 tasks query ASIC capabilities directly; V2 reads device
  configuration; `create_jobs_task.c` switches on concrete V1/V2 payload types;
  `asic_result_task.c` combines ASIC polling, self-test, monitoring, scoreboard,
  and V1/V2 share submission.
- **Target seam:** Stratum publishes neutral work envelopes and implements a
  source-identified share sink. Capability snapshots are injected. Mining owns
  all work execution and result fan-out.

## Relationships to other feature slices

| Related feature | Relationship |
|---|---|
| [Stratum session and message safety](../stratum-session-safety/SPEC.md) | Each client session delegates protocol parsing, bounds, and transport semantics to this slice. |
| [Mining work and result pipeline](../work-pipeline/SPEC.md) | Receives neutral work and returns source-identified share candidates without exposing ASIC backends. |
| [Feature ownership and dependency boundaries](../../architecture/feature-ownership/SPEC.md) | Forbids protocol policy from reaching into concrete devices or composition state. |
| [Thermal, fan, and overheat control](../../device-safety/thermal-fan-overheat/SPEC.md) | Consumes all-pools-unavailable state to stop power safely. |
| [Settings validation and persistence](../../configuration/settings-persistence/SPEC.md) | Owns validation and migration of pool/protocol settings. |

## Verification approach

- Unit-test the coordinator with injected queue/task/probe outcomes and a fake
  protocol task; test typed queue ownership independently; then run repeated
  reconnect/failover transitions while tracking heap and task count.
- **Exact revision:** `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f`
- **Evidence classes:** source/test-surface reconciliation only in this change.

## Constraint provenance

- **Current repository evidence:** Two-slot failover, sticky manual fallback,
  automatic recovery, all-pools-unavailable power state, shared socket helpers,
  and system job cleanup are present; task-start propagation, typed queue
  ownership, V1 destruction, and coherent mining-parameter transfer are not.
- **Historical review evidence:** In merged [#717](https://github.com/bitaxeorg/ESP-Miner/pull/717), mutatrum established `pool`/`fallback pool` naming and marked a pool-settings struct/n>2 only as future consideration. Merged [#913](https://github.com/bitaxeorg/ESP-Miner/pull/913) put suggested difficulty after authorization; WantClue raised synchronization while mutatrum's no-mutex conclusion was specifically based on the then-`uint32_t` value. Merged [#1553](https://github.com/bitaxeorg/ESP-Miner/pull/1553) resolved the narrow public coordinator-header and focused-scope constraints. Merged [#1722](https://github.com/bitaxeorg/ESP-Miner/pull/1722) contains an unresolved/outdated extraction suggestion now corroborated by current shared socket/system cleanup code. Merged [#1422](https://github.com/bitaxeorg/ESP-Miner/pull/1422) explored queue-less work but did not establish it as the durable architecture.
- **Disposition:** Accepted constraints are applied where resolved and current;
  future, unresolved, outdated, and open PR #1855 architecture is not promoted.
