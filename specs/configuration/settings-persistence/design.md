# Settings validation and persistence — design

## Components and responsibilities

| Component | Responsibility | Implementation pointer |
|---|---|---|
| Settings descriptor/cache | NVS name, REST name, type, default, bounds, cached value, migration | `main/nvs_config.c`, `main/nvs_config.h` |
| HTTP settings handler | Parse once, prevalidate whole request, then apply | `main/http_server/http_server.c` (`check_settings_and_update`) |
| Hardware profile adapter | Compose board-specific voltage, frequency, sensor, and fan limits | Current `main/device_config.c`, `main/device_config.h`; target immutable profile |
| Public schema and client | External field domains and generated types | `main/http_server/openapi.yaml`, `main/http_server/axe-os/src/app/generated/` |

## Authority and dependency direction

| Fact or state | Authoritative owner | Consumers | Forbidden duplicate |
|---|---|---|---|
| Generic storage type/default/bounds | NVS settings descriptor | HTTP handler, system serialization | Hand-coded parallel bounds in AxeOS |
| Narrow enum/domain rule | Firmware field validator and OpenAPI schema | Generated client and UI control | UI-only enforcement |
| Board tuning limits | `DeviceConfig`/thermal capability APIs | Settings validator and AxeOS hints | ASIC-model switches in HTTP or UI |
| Persistent value | NVS configuration layer | Domain tasks and API serializers | Browser local storage as device authority |

## Interfaces and contracts

- PATCH `/api/system` is atomic with respect to validation: validate all
  supplied fields before any requested setting is queued for persistence.
- A syntactically valid JSON value with an invalid type/domain returns HTTP 400.
- The public API cannot enable overheat mode; internal safety logic owns entry.
- Bool storage is U16 0/1. Float storage remains string-backed where factory
  `config.csv` seeding requires it. Key renames and type changes need migration.
- Browser-only chart, sorting, layout, and display-label choices stay outside NVS.

## Cross-layer change matrix

| Surface | Required reconciliation |
|---|---|
| Firmware validation | Descriptor plus field-specific and hardware-specific checks |
| OpenAPI | Exact type, enum, bounds, units, read/write semantics, error response |
| Generated AxeOS client | Regenerate after schema changes; do not hand-edit |
| AxeOS service/UI | Typed values and controls matching firmware domains |
| Mock data | Values valid under the same schema |
| NVS/defaults/migration | Stable key/type or explicit reader, writer mirror, and retirement plan |
| Tests | Multi-field atomic rejection, boundary values, migrations, downgrade cases |

## Required and forbidden behavior

- Validate types before reading `valueint`, `valuedouble`, or `valuestring`.
- Bounds-check enum keys and indexed settings before descriptor/value access.
- Never apply valid siblings from a request containing an invalid field.
- Never rename an NVS key or change its representation without an explicit
  upgrade/downgrade decision.
- Never persist frontend-only preferences merely to share the settings path.

## Failure, ownership, and lifecycle boundaries

- Parsed JSON and temporary strings are request-owned and freed on all exits.
- NVS writes are queued only after validation succeeds; queue/write/commit
  failures must be logged and surfaced where the API can do so.
- A migration reads the legacy representation before erasing or replacing it.
- A restart requirement must be returned to the client, not inferred only in UI.

## Compatibility and resources

- Legacy frequency and manual-fan keys remain mirrored while downgrade support
  requires them; removal needs a dated compatibility decision.
- Avoid redundant flash writes. Runtime or browser state does not trigger NVS
  writes unless the user changed a persistent setting.

## Current implementation boundary

- **Conforming:** NVS descriptors and two-pass HTTP validation centralize most
  persistent setting authority.
- **Violations:** consumers receive concrete `DeviceConfig` and may branch on
  model/backend details instead of a narrow immutable settings/safety profile.
- **Target seam:** composition derives immutable hardware profiles; settings
  policy validates against their portable limits while NVS and HTTP remain
  persistence/transport adapters.

## Relationships to other feature slices

| Related feature | Relationship |
|---|---|
| [Hostname, mDNS, and Origin safety](../../networking/hostname-mdns-origin/SPEC.md) | Hostname is persistent input but mDNS conflict identity is runtime state. |
| [Thermal, fan, and overheat control](../../device-safety/thermal-fan-overheat/SPEC.md) | Settings validation consumes hardware and safety limits owned there. |
| [Dashboard statistics selection](../../axeos/dashboard-statistics/SPEC.md) | Chart selection is explicitly browser-local, not NVS state. |
| [Feature ownership and dependency boundaries](../../architecture/feature-ownership/SPEC.md) | Separates portable setting policy from board-profile, NVS, and HTTP adapters. |

## Verification approach

- Add table-driven validation and migration tests, regenerate the API client,
  and run focused AxeOS tests before a settings contract is called complete.
- **Exact revision:** `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f`
- **Evidence classes:** source/schema reconciliation only in this change.

## Constraint provenance

- **Current repository evidence:** Two-pass validation and NVS compatibility
  code corroborate most constraints; OpenAPI rotation currently contradicts it.
- **Historical review evidence:** [PR #880](https://github.com/bitaxeorg/ESP-Miner/pull/880) (mutatrum, merged; atomic 400 and per-field domains), [PR #1051](https://github.com/bitaxeorg/ESP-Miner/pull/1051) (mutatrum, merged/resolved; frequency downgrade mirror), [PR #1236](https://github.com/bitaxeorg/ESP-Miner/pull/1236) (WantClue, merged/resolved; indexed bounds), and [PR #1331](https://github.com/bitaxeorg/ESP-Miner/pull/1331) (WantClue, merged; explicit legacy-key migration). PR #1152 was closed unmerged and is used only as corroborating rationale for representations visible in current code.
- **Disposition:** Merged feedback plus current code defines the constraint;
  unmerged discussion does not independently establish policy.
