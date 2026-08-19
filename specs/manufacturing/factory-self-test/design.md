# Factory self-test lifecycle — design

## Components and responsibilities

| Component | Responsibility | Implementation pointer |
|---|---|---|
| Self-test policy | State machine, applicability, pass/fail decisions, owned messages, cleanup | Target extraction from `main/self_test/self_test.c` |
| Qualification profile | Immutable applicable checks, thresholds, and required capabilities | Target board-profile contract composed from `main/device_config.c` and `main/nvs_config.c` |
| Hardware ports | Power, thermal, fan, ASIC diagnostic work, reset, and metrics snapshots | Target interfaces over `main/thermal/thermal.c`, `main/power/`, and `components/asic/` |
| Platform adapters | Button/reset event, persistence, restart, time, and synchronization | Current ESP/NVS integration in `main/self_test/self_test.c` and `main/nvs_config.c` |
| Status consumers | Consume a stable copied self-test snapshot | `main/screen.c`, `main/http_server/http_server.c` |

## Authority and dependency direction

| Fact or state | Owner | Consumers | Forbidden duplicate |
|---|---|---|---|
| Self-test phase/result/message buffers | Self-test module | Display/API | Pointer to caller stack storage |
| Reset semaphore | Self-test lifecycle | Button/reset callback and terminal wait | Use before successful creation |
| Hardware thresholds/capabilities | Immutable qualification profile | Self-test policy | Test-local board-model assumptions |
| Safe power/reset state | Safety and ASIC driver ports | Self-test terminal handler | Direct partial cleanup paths |
| Diagnostic work identity and provenance | Mining work pipeline | Self-test result observer | Stratum job IDs or mutable ASIC difficulty |

## Interfaces and cross-layer matrix

| Surface | Required reconciliation |
|---|---|
| Firmware | Explicit state/terminal transitions, copied messages, cleanup |
| Display/API | Stable message/result lifetime and clear failure reason |
| NVS | Factory/manual flag, thresholds, pass/fail clearing policy |
| Tests | Failure injection, lifetime, cleanup, all terminal paths |
| HIL | Fan, sensors, voltage, power, hashrate domains, reset/restart |

## Required and forbidden behavior

- Copy retained strings into bounded module-owned arrays; never store a pointer
  to `logString`, countdown buffers, or other caller stack memory.
- Create/check synchronization before registering or enabling its producer.
- After a fatal failure, explicitly stop the flow; do not rely on an implicitly
  non-returning helper unless its contract is declared and enforced.
- Check allocations and hardware calls before using their outputs.
- Use shared thermal/power/ASIC interfaces and restore safe-off on every terminal
  path.
- Submit hash diagnostics through the neutral mining work service with explicit
  purpose and provenance. Never invoke a protocol parser or transport queue.
- Keep ESP/NVS/GPIO/task details in adapters; the policy depends only on ports
  and owned value objects.

## Failure, ownership, and lifecycle boundaries

- The self-test module owns its message/result/finished storage for the whole
  active and terminal display lifetime.
- Domain averages and parsed work are freed exactly once after ownership ends.
- The reset semaphore exists until restart or explicit self-test teardown.
- Terminal pass/fail is idempotent or guarded so concurrent failures cannot run
  cleanup/restart logic twice.

## Compatibility and resources

- Thresholds are board-capability/default driven and changes need factory
  process/version coordination.
- Buffers are fixed and bounded; monitoring loops and warmup/cooling require
  explicit time limits or documented operator-reset behavior.

## Current implementation boundary

- **Conforming:** terminal safe-off, semaphore checks, and some allocation
  handling already exist in the self-test lifecycle.
- **Violations:** `main/self_test/self_test.c` is simultaneously policy,
  hardware adapter, ESP task integration, persistence client, mining producer,
  and display-state publisher. It includes concrete board/driver APIs and
  reaches through `GlobalState`.
- **Target seam:** extract a pure self-test state machine driven by an immutable
  qualification profile and injected ports. Board/ESP adapters are selected in
  the composition root; hash diagnostics use the neutral mining work pipeline.

## Relationships to other feature slices

| Related feature | Relationship |
|---|---|
| [Thermal, fan, and overheat control](../../device-safety/thermal-fan-overheat/SPEC.md) | Supplies safe actuation, sensor authority, and terminal safe-off. |
| [On-device display lifecycle](../../device-ui/display-lifecycle/SPEC.md) | Consumes stable self-test messages in a pre-carousel state. |
| [Feature ownership and dependency boundaries](../../architecture/feature-ownership/SPEC.md) | Defines policy, port, adapter, and composition rules. |
| [ASIC driver contract](../../asic/driver-contract/SPEC.md) | Supplies purpose-aware diagnostic lifecycle and results without exposing a concrete backend. |
| [Mining work and result pipeline](../../mining/work-pipeline/SPEC.md) | Owns diagnostic work identity, provenance, dispatch, and result observation without Stratum. |

## Verification approach

- Extract message/state transitions for unit tests, inject allocation/semaphore
  and hardware failures, then run authorized factory-board tests with a recovery
  image and serial log capture.
- **Exact revision:** `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f`
- **Evidence classes:** source reconciliation only in this change.

## Constraint provenance

- **Current repository evidence:** Semaphore checks and terminal safe-off exist;
  `SELF_TEST_MODULE.message = msg` and countdown assignment retain stack-backed
  pointers, and several failure sites do not explicitly return after terminal
  handling.
- **Historical review evidence:** [PR #1615](https://github.com/bitaxeorg/ESP-Miner/pull/1615) was merged, but the relevant WantClue threads about message ownership,
  semaphore initialization, and returning after initialization failure remain
  unresolved (some outdated).
- **Disposition:** These are explicit implementing safety constraints and known
  current gaps, not claims that the historical threads were resolved.
