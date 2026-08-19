# Settings validation and persistence — risks and scope

## Scope

### In

- Public settings validation, atomic update semantics, NVS representation,
  defaults, migrations, and cross-layer schema agreement.

### Out

- Board-specific tuning values and domain-task behavior after a valid update.
- Browser-only dashboard and sorting preferences.

### Review unit

- **Why this is the smallest coherent change:** Validation and persistence form
  one transaction contract; changing either alone can create partial writes.
- **Work intentionally split out:** Hardware safety control, hostname runtime
  identity, and individual UI feature behavior have separate specs.

## Assumptions and open questions

- Supported downgrade depth must be stated before retiring any fallback key.
- Decide whether persistence queue/commit failure can be synchronously reported
  by PATCH or needs an explicit asynchronous status contract.
- Correct the OpenAPI rotation enum and regenerate the client.

## Failure modes

| Failure | Impact | Detection | Mitigation or recovery |
|---|---|---|---|
| Partial request applied | Unsafe mixed configuration | Multi-field negative test and NVS snapshot | Prevalidate all fields before writes |
| Silent key/type change | Lost settings or failed downgrade | Upgrade/downgrade fixture | Explicit migration and temporary mirror |
| Schema drift | Valid client request rejected or unsafe value offered | Generation plus contract test | Reconcile all layers in one change |
| Invalid index/key | NULL dereference or memory error | Boundary unit test | Reject before descriptor access |

## Ownership and boundary risks

- **Shadow state:** UI validators are usability aids, never the safety authority.
- **Allocation/lifetime:** Request strings and JSON remain request-owned.
- **Untrusted input:** JSON type, range, enum, string length, and hardware bounds
  are checked before conversion or persistence.
- **Concurrency/storage:** Queueing after validation preserves request atomicity
  only if no earlier side effect occurs.
- **Cross-layer drift:** The present rotation enum contradiction is the known gap.

## Security, safety, and resources

- Voltage, frequency, fan, thermal, Wi-Fi, certificate, and protocol settings
  are safety/security relevant even when submitted from the local network.
- Limit write amplification and do not persist runtime conflict resolution.

## Rollout and rollback

- Ship migrations before writers stop maintaining any required legacy format;
  prove both upgrade and supported downgrade before removing compatibility code.
