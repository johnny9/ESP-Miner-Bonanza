# Dashboard statistics selection — design

## Components and responsibilities

| Component | Responsibility | Implementation pointer |
|---|---|---|
| Statistics task | Own sampled history and capacity | `main/tasks/statistics_task.c`, `main/tasks/statistics_task.h` |
| HTTP handler | Select requested columns and serialize labels/rows | `main/http_server/http_server.c` (`GET_system_statistics`) |
| OpenAPI | Public query and response shape | `main/http_server/openapi.yaml` |
| AxeOS service/mock | Build column list and preserve response contract | `main/http_server/axe-os/src/app/services/system.service.ts` |
| Dashboard | Persist presentation choice and map labels to axes | `main/http_server/axe-os/src/app/components/home/home.component.ts` |

## Authority and dependency direction

| Fact or state | Owner | Consumers | Forbidden duplicate |
|---|---|---|---|
| Telemetry series key/order | Firmware API response labels | Service and dashboard | Position-only mapping without labels |
| Human display label/unit | AxeOS enum/presentation mapping | Dashboard | Firmware persistence of display text |
| Y1/Y2 selection | Browser local storage | Dashboard and request builder | NVS field or firmware UI preference |
| History limit | Firmware system info/statistics task | Dashboard pruning | Unbounded browser arrays |

## Interfaces and cross-layer matrix

| Surface | Required reconciliation |
|---|---|
| Firmware | Stable key mapping and bounded selected serialization |
| OpenAPI | `columns` array and ordered labels/data schema |
| Generated client | Regenerate when query or response shape changes |
| AxeOS | Key-to-label/unit mapping and optional-series filtering |
| Mock | Same labels and row order as a real response |
| NVS | Not applicable; selections are browser-local |
| Tests | Selection persistence, ordering, deduplication, unknown/optional series |

## Required and forbidden behavior

- Always include the timestamp and return its key in `labels`.
- Ignore or explicitly reject unknown requested keys; never shift known values
  into an unlabeled position.
- Keep keys URL/JSON safe and stable across display-text changes.
- Do not write NVS when a user changes axes.

## Failure, ownership, and resources

- The HTTP request owns its decoded query buffer and JSON response.
- AxeOS unsubscribes/replaces prior history loads when the selection changes.
- Requested-column filtering reduces serialization and network work; both
  firmware history and browser arrays remain bounded.

## Current implementation boundary

- **Conforming:** statistics ownership, HTTP selection, OpenAPI keys, and
  browser-local presentation choices follow the intended direction.
- **Violations:** none identified in the reviewed boundary; the HTTP handler is
  still an application adapter and must not become the telemetry authority.
- **Target seam:** keep a typed immutable statistics snapshot between the
  statistics task and HTTP serializer; AxeOS consumes only the public API.

## Relationships to other feature slices

| Related feature | Relationship |
|---|---|
| [Settings validation and persistence](../../configuration/settings-persistence/SPEC.md) | Defines why chart selection is not device-persistent configuration. |
| [Feature ownership and dependency boundaries](../../architecture/feature-ownership/SPEC.md) | Keeps telemetry authority separate from HTTP and AxeOS presentation adapters. |

## Verification approach

- Use firmware serialization cases plus AxeOS service/component tests with
  reordered, duplicate, optional, and unknown columns.
- **Exact revision:** `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f`
- **Evidence classes:** source/schema reconciliation only in this change.

## Constraint provenance

- **Current repository evidence:** Local storage, stable enum keys, selective
  query generation, ordered response labels, mock parity, and bounded arrays
  are present.
- **Historical review evidence:** [PR #955](https://github.com/bitaxeorg/ESP-Miner/pull/955) (mutatrum, merged; all 12 relevant threads resolved or outdated) established browser-local selection, stable identifiers, selective API columns, response ordering, and OpenAPI examples.
- **Disposition:** Accepted and corroborated by current code.
