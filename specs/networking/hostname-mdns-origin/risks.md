# Hostname, mDNS, and Origin safety — risks and scope

## Scope and review unit

- **In:** Hostname validation/persistence, mDNS conflict lifecycle, runtime API
  identity, Origin authorization, redirects, and swarm identity fallback.
- **Out:** General Wi-Fi provisioning and subnet scan algorithms.
- **Smallest coherent change:** These surfaces share identity semantics; fixing
  only one can break redirects, discovery, or authorization.
- **Split out:** General settings transaction and swarm data compatibility.

## Assumptions and open questions

- Define exact same-device Origin policy for IP access, IPv6 literals, HTTPS,
  reverse proxies, and approved development ports.
- Resolve whether conflict suffixes remain stable only for one boot or are
  cached in volatile state across reconnects.

## Failure modes

| Failure | Impact | Detection | Mitigation |
|---|---|---|---|
| Invalid/oversize name | mDNS/DHCP failure or truncation | Boundary table | RFC 1123 plus full-length budget |
| Event callback blocks | Wi-Fi instability/watchdog | Timing test | Worker task with finite probes |
| Runtime suffix persisted | Surprise identity/flash writes | NVS spy test | Runtime cache only |
| Broad Origin accepted | Browser-driven local CSRF | Origin matrix | Match explicit device identity |
| Allocation/task failure | Crash or stuck in-progress state | Failure injection | Check, clean up, allow retry |

## Security, resources, and rollout

- Origin checks are defense-in-depth for browser requests and do not replace
  authentication when remote exposure is supported.
- Bound host buffers, parsing, probes, task stack, retries, and NVS writes.
- Tightening validation needs migration behavior for an already-stored invalid
  hostname and a recovery path through AP mode or a safe default.
