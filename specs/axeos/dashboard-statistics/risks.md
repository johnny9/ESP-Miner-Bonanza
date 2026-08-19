# Dashboard statistics selection — risks and scope

## Scope and review unit

- **In:** Series IDs, Y1/Y2 browser persistence, selective statistics query,
  response ordering, mock parity, and bounded chart data.
- **Out:** Sampling algorithms, live WebSocket transport, and chart styling.
- **Smallest coherent change:** A new series crosses API key, response mapping,
  service/mock, UI units, and tests; those surfaces move together.
- **Split out:** Sampling cadence changes belong to statistics collection.

## Assumptions and open questions

- Stable keys remain backward compatible even when display labels change.
- AC-05 needs focused automated evidence at the current revision.

## Failure modes

| Failure | Impact | Detection | Mitigation |
|---|---|---|---|
| Label/order drift | Wrong metric plotted | Reordered-response test | Map by returned labels |
| UI label used as key | Saved choices break on rename | Persistence test | Stable enum key |
| Selection written to NVS | Flash wear and device-global preference | NVS write audit | Browser local storage only |
| Unbounded points | Browser/device memory pressure | Long-history test | Enforce `statsLimit` |

## Ownership, security, and rollout

- Query keys are untrusted and must be bounded to the known series table.
- No credentials or private configuration belong in telemetry history.
- Add keys compatibly; removing or renaming a key needs browser-storage and API
  migration behavior.
