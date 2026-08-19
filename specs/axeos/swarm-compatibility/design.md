# Swarm mixed-version compatibility — design

## Components and responsibilities

| Component | Responsibility | Implementation pointer |
|---|---|---|
| Generated API client | Typed calls to the device serving AxeOS | `main/http_server/axe-os/src/app/generated/` |
| System API service | Local/remote transport split and mocks | `main/http_server/axe-os/src/app/services/system.service.ts` |
| Swarm component | Discovery, fan-out, compatibility normalization, storage, UI | `main/http_server/axe-os/src/app/components/swarm/swarm.component.ts` |
| Local storage | Browser-owned peer list, sort/view/refresh preferences | `main/http_server/axe-os/src/app/local-storage.service.ts` |

## Authority and dependency direction

| Fact or state | Owner | Consumers | Forbidden duplicate |
|---|---|---|---|
| Local exact API schema | OpenAPI/generated client | Local calls | Assuming every remote peer matches it |
| Remote connection address | Swarm browser record | Fetch/action/link code | Replacing it with display name |
| Peer device semantics | Remote firmware payload | Normalizer and UI | UI inventing hardware facts |
| Sort/view/refresh choice | Browser local storage | Swarm UI | NVS persistence |

## Interfaces and cross-layer matrix

| Surface | Required reconciliation |
|---|---|
| Firmware/OpenAPI | New fields additive when mixed-version peers are supported |
| Generated client | Used for local exact-schema operations |
| AxeOS service | Own remote URI, timeout, error, and mock paths |
| Swarm UI | Guard optional fields and normalize old alternatives |
| Persistence | Browser local only; store stable connection address/version |
| Tests | Old/new payload matrix, failures, identity fallback, recovery |

## Required and forbidden behavior

- Bound concurrent fan-out and each peer operation; one failure cannot abort all.
- Preserve loose/optional peer typing at the compatibility boundary.
- Keep hardware facts sourced from the peer, not inferred from UI labels except
  for an explicit legacy fallback mapping.
- Do not send remote calls through a local generated-client base URL by accident.

## Failure, ownership, and compatibility

- Observable subscriptions and refresh timers are disposed with the component.
- Errors retain enough peer identity to recover on the next refresh.
- Prefer `fullHostname`, then configured hostname, then IP/connection address
  according to the current access mode; missing fields are normal for old peers.

## Current implementation boundary

- **Conforming:** the browser owns remote compatibility, connection addresses,
  optional-field normalization, fan-out, and browser preferences.
- **Violations:** none identified in the reviewed module boundary; legacy
  fallbacks must remain compatibility adapters rather than firmware authority.
- **Target seam:** preserve the system service as the remote transport adapter
  and the component as presentation policy over normalized peer values.

## Relationships to other feature slices

| Related feature | Relationship |
|---|---|
| [Hostname, mDNS, and Origin safety](../../networking/hostname-mdns-origin/SPEC.md) | Supplies configured/runtime identities and HTTP authorization constraints. |
| [Feature ownership and dependency boundaries](../../architecture/feature-ownership/SPEC.md) | Defines the remote compatibility adapter and presentation boundary. |

## Verification approach

- Use service/component fixtures for multiple firmware payload vintages and
  virtual time for timeout, failure, retry, and recovery behavior.
- **Exact revision:** `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f`
- **Evidence classes:** source reconciliation only in this change.

## Constraint provenance

- **Current repository evidence:** Local/remote paths, five-second scan timeout,
  fallback model/fields, stable connection address, and access-mode links exist.
- **Historical review evidence:** [PR #1503](https://github.com/bitaxeorg/ESP-Miner/pull/1503) (mutatrum, merged; all five relevant threads resolved) established the service boundary, remote URI, mock, timeout, and missing-field guards. [PR #1202](https://github.com/bitaxeorg/ESP-Miner/pull/1202) (mutatrum, merged) records intentional loose swarm typing for mixed versions.
- **Disposition:** Accepted and corroborated. Open PR #1737 feedback about a
  future `notAccessible` flag is not promoted into this contract.
