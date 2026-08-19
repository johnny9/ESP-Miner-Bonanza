# Factory self-test lifecycle — risks and scope

## Scope and review unit

- **In:** State/message lifetime, synchronization, prerequisite failure, hardware
  checks, measurements, terminal cleanup, NVS flag, restart/reset, and evidence.
- **Out:** Production threshold selection, protocol parsing, and domain-driver
  internals.
- **Smallest coherent change:** Fix message ownership plus explicit terminal flow
  with focused tests; do not mix unrelated self-test algorithm changes.
- **Split out:** Board threshold changes and new diagnostics can be separate.

## Assumptions and open questions

- Choose fixed maximum lengths for message/result/finished buffers and define
  truncation behavior.
- Declare whether `tests_done` is formally non-returning or replace it with an
  explicit transition/return pattern.
- Define bounded warmup/test duration and behavior if reset input is unavailable.

## Failure modes

| Failure | Impact | Detection | Mitigation |
|---|---|---|---|
| Stack string retained | Corrupt display/log or use-after-scope | Lifetime regression | Module-owned bounded copy |
| Semaphore unavailable | NULL use or unrecoverable failure screen | Creation injection | Check before enable/use |
| Flow continues after fatal error | Unsafe hardware operations | Failure-path test | Explicit return/terminal state |
| Cleanup runs twice | Double free or repeated power transition | Concurrent-failure test | Idempotent guarded terminal handler |
| Test hangs unbounded | Manufacturing stall/hot device | Virtual-time/HIL | Timeouts and safe-off |
| Self-test reuses Stratum as a test harness | Protocol state and diagnostic policy become coupled | Dependency/architecture test | Neutral purpose-tagged mining work |
| Policy calls concrete board/ESP APIs | New boards require feature edits and failure tests cannot isolate policy | Include/dependency check | Injected ports plus composition-root adapters |

## Security, resources, and rollout

- Self-test output must not include Wi-Fi/pool secrets or device-private material.
- Bound buffers, allocations, task stack, loop duration, and NVS writes.
- Preserve a known recovery image and explicit reset path before factory rollout;
  never flash a physical unit without authorization.
