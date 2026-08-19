# Best-share scoreboard — design

## Components and responsibilities

| Component | Responsibility | Implementation pointer |
|---|---|---|
| Scoreboard task module | Sorted bounded state, lock, qualification, persistence | `main/tasks/scoreboard.c`, `main/tasks/scoreboard.h` |
| NVS configuration | Safe indexed key/value access | `main/nvs_config.c` |
| HTTP/OpenAPI | Snapshot serialization and wire contract | `main/http_server/http_server.c`, `main/http_server/openapi.yaml` |
| AxeOS | Fetch timeout, sorting, derived rank/age | `main/http_server/axe-os/src/app/components/scoreboard/` |

## Authority and dependency direction

| Fact or state | Owner | Consumers | Forbidden duplicate |
|---|---|---|---|
| Ordered top-share entries | Scoreboard module under its mutex | NVS writer and HTTP snapshot | HTTP reading mutable entries unlocked |
| Persistent slot mapping | NVS settings descriptor | Scoreboard | Unchecked computed key/index |
| Rank and relative age | AxeOS presentation | Scoreboard component | OpenAPI requiring fields firmware never sends |

## Interfaces and cross-layer matrix

| Surface | Required reconciliation |
|---|---|
| Firmware | Atomic qualification/mutation and consistent snapshot |
| OpenAPI | Actual difficulty/job/extranonce/ntime/nonce/version payload |
| Generated client | Regenerate after schema correction |
| AxeOS | Five-second fetch timeout and presentation-derived fields |
| Mock | Same raw wire fields; derived fields added in the same layer |
| NVS | Bounded indexed records and malformed-record handling |
| Tests | Concurrency, bounds, full-list ordering, schema parity, errors |

## Required and forbidden behavior

- Lock before any read used to decide or perform a mutation; unlock on every exit.
- Check every allocation/object creation before use.
- Send an HTTP error response and return normally after response failure handling.
- Never expose schema-required fields that only a browser derives unless the
  firmware actually serializes them.

## Failure, ownership, and resources

- The scoreboard owns entries and mutex; HTTP owns only its JSON snapshot.
- Persisted strings are parsed into bounded fields and freed after use.
- The list and NVS slot count are fixed; frontend polling and request timeout
  are bounded.

## Current implementation boundary

- **Conforming:** scoreboard state, ordering, locking, and persistence are
  concentrated in the scoreboard module; HTTP and AxeOS are consumers.
- **Violations:** `main/tasks/asic_result_task.c` calls the scoreboard directly
  while also owning ASIC polling, protocol submission, monitoring, and
  self-test result behavior.
- **Target seam:** make scoreboard an observer of neutral validated mining
  candidate events; compose its persistence adapter independently of ASIC and
  Stratum tasks.

## Relationships to other feature slices

| Related feature | Relationship |
|---|---|
| [Mining work and result pipeline](../../mining/work-pipeline/SPEC.md) | Publishes validated candidate events to the scoreboard observer. |
| [Feature ownership and dependency boundaries](../../architecture/feature-ownership/SPEC.md) | Keeps observability independent from ASIC polling and protocol submission. |

## Verification approach

- Add unit tests around a mockable persistence boundary and an API contract test
  for exact payload fields; exercise concurrent reader/writer scheduling.
- **Exact revision:** `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f`
- **Evidence classes:** source/schema reconciliation only in this change.

## Constraint provenance

- **Current repository evidence:** Read-side locking, NVS bounds, HTTP error
  response, and frontend timeout are present; pre-lock mutation reads and schema
  drift contradict the full contract.
- **Historical review evidence:** [PR #1236](https://github.com/bitaxeorg/ESP-Miner/pull/1236) (WantClue and mutatrum, merged; 16/16 relevant threads resolved, many outdated) requested locked struct access, allocation checks, indexed NVS bounds, correct HTTP failure handling, reusable OpenAPI schemas, and a five-second AxeOS timeout.
- **Disposition:** Accepted, with two current implementation gaps recorded.
