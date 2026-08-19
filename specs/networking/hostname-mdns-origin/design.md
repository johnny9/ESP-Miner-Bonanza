# Hostname, mDNS, and Origin safety — design

## Components and responsibilities

| Component | Responsibility | Implementation pointer |
|---|---|---|
| Settings/NVS | Persist only operator-selected base hostname | `main/nvs_config.c`, `main/http_server/http_server.c` settings PATCH handler |
| Connect/mDNS worker | Apply DHCP name, probe conflicts, advertise runtime name | `components/connect/connect.c` |
| Identity status snapshot | Cache configured, mDNS, and full hostname | Current `GlobalState.SYSTEM_MODULE`; target typed networking status sink/snapshot |
| HTTP security | Parse and authorize peer/Origin, return cached identity | `main/http_server/http_server.c` |
| AxeOS swarm | Choose compatible links without owning identity | `main/http_server/axe-os/src/app/components/swarm/swarm.component.ts` |

## Authority and dependency direction

| Fact or state | Owner | Consumers | Forbidden duplicate |
|---|---|---|---|
| Configured base hostname | NVS settings | DHCP, mDNS worker, system API | Conflict probe silently overwriting NVS |
| Active mDNS hostname | Connect/mDNS runtime state | System API and AxeOS | API handler probing mDNS on demand |
| Allowed browser Origin | HTTP security policy using device identities | All protected handlers | “Any private/.local host” shortcut |
| Connection fallback | AxeOS swarm record | Link rendering and remote calls | Firmware storing browser route choice |

## Interfaces and cross-layer matrix

| Surface | Required reconciliation |
|---|---|
| Firmware | RFC 1123 validator, async mDNS, runtime cache, strict Origin parser |
| OpenAPI | Hostname pattern/max length and configured/runtime field meanings |
| Generated client | Regenerate after schema clarification |
| AxeOS | Input feedback, redirect handling, link fallback including dev ports |
| Mock | Distinct configured, mDNS, full, and IPv4 values |
| NVS | Base hostname only; no conflict-suffix writes |
| Tests | Validation table, async failure, conflict, Origin matrix, compatibility |

## Required and forbidden behavior

- Check pointers, allocations, task creation, interface lookup, and mDNS calls.
- Never block a Wi-Fi event callback with multi-second conflict probing.
- Never persist a runtime conflict suffix without an explicit user settings action.
- Never treat arbitrary private IP, `.local`, or bare-host Origins as same-device.
- Preserve explicit development-port support without weakening host validation.

## Failure, ownership, and lifecycle boundaries

- The mDNS worker owns probe strings and frees them on every path.
- Cached runtime identity becomes valid only after successful `mdns_hostname_set`.
- Task creation failure clears the in-progress guard and permits a later retry.
- Hostname changes preserve the previous configured name until the request has
  passed full settings validation.

## Compatibility and resources

- Older peers may expose only configured hostname or IP; consumers fall back
  without assuming `fullHostname` or `mdnsHostname` exists.
- Probes have finite attempts/delays and no unbounded allocation or flash writes.

## Current implementation boundary

- **Conforming:** `components/connect/connect.c` owns mDNS work and the HTTP
  layer consumes cached identity instead of probing synchronously.
- **Violations:** the reusable connect component receives/updates
  `GlobalState`, coupling networking to application-private storage.
- **Target seam:** inject an identity status sink and immutable configured-name
  value into the connect adapter; composition maps snapshots to system/API state.

## Relationships to other feature slices

| Related feature | Relationship |
|---|---|
| [Settings validation and persistence](../../configuration/settings-persistence/SPEC.md) | Owns atomic validation and the persistent base hostname. |
| [Swarm mixed-version compatibility](../../axeos/swarm-compatibility/SPEC.md) | Consumes cached identity with compatible fallbacks. |
| [Feature ownership and dependency boundaries](../../architecture/feature-ownership/SPEC.md) | Replaces application-state coupling with a networking status port and composition adapter. |

## Verification approach

- Extract pure hostname/Origin parsing for table-driven tests; inject mDNS/NVS
  operations to prove async failure and no implicit persistence.
- **Exact revision:** `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f`
- **Evidence classes:** source/schema reconciliation only in this change.

## Constraint provenance

- **Current repository evidence:** Async initialization and cached API state are
  present; runtime conflict names still persist and Origin checks are broad.
- **Historical review evidence:** [PR #1240](https://github.com/bitaxeorg/ESP-Miner/pull/1240) (WantClue and mutatrum, merged; mixed resolved/outdated/unresolved threads) supplied allocation, event-loop, hostname-length, Origin/CSRF, cache, and fallback concerns. [PR #1865](https://github.com/bitaxeorg/ESP-Miner/pull/1865) is open and independently proposes RFC 1123 validation and runtime-only conflict names.
- **Disposition:** Merged, corroborated constraints are requirements. The two
  open/currently contradicted items are explicit implementing criteria, not
  claims of existing support or accepted thread resolution.
