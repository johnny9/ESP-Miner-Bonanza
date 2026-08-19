# Best-share scoreboard — risks and scope

## Scope and review unit

- **In:** Qualification, ordering, lock boundary, persistence, HTTP payload,
  five-second frontend fetch, and schema parity.
- **Out:** Share validation and visual table styling.
- **Smallest coherent change:** Runtime state, NVS slots, and wire schema describe
  the same bounded records and must be reviewed together.
- **Split out:** General Stratum parsing and chart telemetry have separate specs.

## Assumptions and open questions

- Decide tie stability and behavior if one shifted NVS write fails.
- Decide whether `rank` and `since` are removed from OpenAPI required fields or
  added by firmware; current behavior treats them as AxeOS presentation.

## Failure modes

| Failure | Impact | Detection | Mitigation |
|---|---|---|---|
| Pre-lock read races | Wrong qualification or torn list | Concurrent test/TSAN-equivalent analysis | Lock full decision and mutation |
| Malformed NVS entry | Lost or corrupt history | Boot parse fixtures | Skip invalid bounded record and log |
| Schema/wire mismatch | Generated-client errors | Contract test | One exact payload definition |
| Lock/allocation failure | Hung or malformed HTTP response | Failure injection | Send 500 and clean up |

## Ownership, security, and resources

- Job IDs and extranonce strings are pool-controlled and must remain bounded.
- The fixed entry count limits RAM, NVS wear, response size, and UI work.
- A format migration must preserve old valid entries or document reset/rollback.
