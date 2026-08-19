# Best-share scoreboard — acceptance

## Functional behavior

- [x] **OBS-SCOREBOARD-AC-01:** The list is bounded to `MAX_SCOREBOARD`, sorted
  by difficulty, and persisted through indexed NVS entries.
- [x] **OBS-SCOREBOARD-AC-02:** HTTP holds the scoreboard mutex for the entire
  read/serialization loop and sends HTTP 500 before returning gracefully on
  lock failure.
- [x] **OBS-SCOREBOARD-AC-03:** Indexed NVS access validates key and index before
  dereferencing storage metadata.
- [x] **OBS-SCOREBOARD-AC-04:** AxeOS requests the endpoint with a five-second
  timeout and derives rank and elapsed age for presentation.
- [ ] **OBS-SCOREBOARD-AC-05:** Qualification checks, insertion, shifting,
  count mutation, and persistence occur under one scoreboard lock. Current
  `scoreboard_add` reads `count` and the last entry before acquiring the lock.

## Interfaces and compatibility

- [ ] **OBS-SCOREBOARD-AC-06:** OpenAPI describes the actual firmware payload.
  Current schema requires `rank` and `since`, but firmware omits them and AxeOS
  derives them.
- [ ] **OBS-SCOREBOARD-AC-07:** Focused tests cover empty/full lists, ties,
  malformed persisted entries, concurrent add/read, allocation/lock failure,
  and exact wire/schema parity.

## Verification evidence

| Evidence class | Exact command, case, revision, target, result, and date |
|---|---|
| Source/schema reconciliation | Inspected scoreboard, NVS indexed access, HTTP handler, OpenAPI, service, and component at `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f` — AC-01 through AC-04 present; AC-05 and AC-06 gaps found, 2026-08-10 |
| Focused regression | Not run; documentation-only reconciliation. AC-07 remains unchecked. |
| AxeOS generation/test/build | Not run; documentation-only reconciliation. |
| Device/HIL | Not run and not required for this documentation change. |

## Acceptance rule

The feature is complete only when mutation and snapshot boundaries are race-free
and the generated public schema exactly matches the firmware payload.
