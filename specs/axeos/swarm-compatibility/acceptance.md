# Swarm mixed-version compatibility — acceptance

## Functional behavior

- [x] **AXE-SWARM-AC-01:** Local-device calls use generated API operations;
  remote-device calls are owned by `SystemApiService`/swarm helpers with an
  explicit URI and mock behavior.
- [x] **AXE-SWARM-AC-02:** Remote scan/refresh work has a five-second aggregate
  timeout and handles errors per peer.
- [x] **AXE-SWARM-AC-03:** Missing newer fields are tolerated and normalized from
  older alternatives instead of rejecting the peer's full payload.
- [x] **AXE-SWARM-AC-04:** Navigation prefers the addressing mode the operator
  used: runtime full hostname for mDNS, configured hostname for bare-host use,
  and IPv4/connection address otherwise.
- [x] **AXE-SWARM-AC-05:** Browser-local swarm state retains a stable
  `connectionAddress` separately from display/advertised identity.
- [ ] **AXE-SWARM-AC-06:** Focused tests cover mixed schemas, missing optional
  fields, mDNS/bare/IP access, development ports, one failed peer, recovery,
  duplicate discovery, and timeout.

## Verification evidence

| Evidence class | Exact command, case, revision, target, result, and date |
|---|---|
| Source reconciliation | Inspected `SystemApiService` and `SwarmComponent` at `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f` — AC-01 through AC-05 present, 2026-08-10 |
| Focused frontend regression | Not run; documentation-only reconciliation. AC-06 remains unchecked. |
| AxeOS build | Not run; no implementation changed. |
| Device/network test | Not run; no private network operation was authorized. |

## Acceptance rule

New peer fields remain optional until a documented compatibility floor makes
them universal, and remote behavior must have focused timeout/error tests.
