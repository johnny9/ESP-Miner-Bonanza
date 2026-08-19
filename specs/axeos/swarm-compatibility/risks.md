# Swarm mixed-version compatibility — risks and scope

## Scope and review unit

- **In:** Local/remote API boundary, mixed-schema normalization, timeouts,
  failure isolation, identity fallback, and browser persistence.
- **Out:** Firmware discovery advertisement and each peer's device behavior.
- **Smallest coherent change:** Remote transport and normalization must change
  together when a peer field becomes required or changes meaning.
- **Split out:** mDNS/Origin security and individual action endpoint semantics.

## Assumptions and open questions

- Define a compatibility floor before deleting a legacy field fallback.
- If an explicit inaccessible-state flag is introduced, define when success
  clears it and add a recovery test; current open review is not yet policy.

## Failure modes

| Failure | Impact | Detection | Mitigation |
|---|---|---|---|
| Missing new field | Peer disappears or UI crashes | Old-payload fixture | Guard and normalize |
| One peer times out | Whole refresh stalls | Virtual-time fan-out test | Per-peer error plus finite aggregate timeout |
| Display name used as route | Actions hit wrong/unreachable host | Identity-mode tests | Stable `connectionAddress` |
| Stale local record | Wrong totals/status | Recovery and version-change test | Refresh normalization and explicit stale state |

## Security, resources, and rollout

- Remote URIs are untrusted browser inputs; validate accepted address forms and
  do not leak one peer's credentials or response into another record.
- Bound fan-out concurrency, response time, subscriptions, and stored peer count.
- Add fields compatibly and keep mocks representative of both missing and
  present optional fields.
