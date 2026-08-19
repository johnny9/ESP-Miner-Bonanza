# On-device display lifecycle — design

## Components and responsibilities

| Component | Responsibility | Implementation pointer |
|---|---|---|
| Device/display profile | Display backend, resolution, offsets, capabilities | Current `main/device_config.c`, `main/device_config.h`; target immutable display descriptor |
| Screen module | Startup layout, screen state, carousel, timers, activity | `main/screen.c` |
| Display adapters | Select and drive local/Bonanza display implementations | `main/display.c`, `main/bonanza_display.c` |
| Settings/OpenAPI/AxeOS | Timeout/rotation choices valid for local backend | `main/nvs_config.c`, `main/http_server/openapi.yaml`, `main/http_server/axe-os/src/app/components/edit/edit.component.ts` |
| External display boundary | Independent firmware layout/sleep behavior | AxeOS `displayBackend` handling in the edit/status components |

## Authority and dependency direction

| Fact or state | Owner | Consumers | Forbidden duplicate |
|---|---|---|---|
| Display backend/resolution | Device/display configuration | `screen_start`, AxeOS capability UI | Re-detecting every update |
| Current screen/activity | LVGL/screen module | Sleep and carousel logic | Parallel inactivity clock |
| System telemetry | Domain state modules | Screen render callbacks | Display-owned stale cache field |
| External display behavior | External display firmware | AxeOS status | ESP-Miner applying LVGL settings to it |

## Interfaces and cross-layer matrix

| Surface | Required reconciliation |
|---|---|
| Firmware | One-time layout, screen states, activity and timer cadence |
| OpenAPI/settings | Timeout units/sentinels and valid rotation values |
| AxeOS | Backend-aware controls and exact sentinel presentation |
| NVS | Signed timeout supporting `-1`, `0`, and positive minutes |
| Tests/HIL | State/timing cases and representative physical displays |

## Required and forbidden behavior

- Select layout once per display initialization; let resolutions own distinct
  screen/carousel composition.
- Keep expensive data refresh out of the fastest presentation callback.
- Do not sleep setup/recovery/pre-carousel screens under the normal carousel rule.
- Reset activity on real interaction or explicitly defined important events.
- Do not impose arbitrary timeout maxima below safe representation/overflow limits.

## Failure, ownership, and resources

- LVGL access occurs under its lock; timer callbacks do not block on network or
  slow hardware operations.
- Display-off state does not stop mining or erase the active screen state.
- Timer and timeout arithmetic uses widths that cannot overflow at accepted max.

## Current implementation boundary

- **Conforming:** backend selection is largely centralized in `main/display.c`,
  and `main/screen.c` owns LVGL presentation lifecycle.
- **Violations:** screen rendering reads broad `GlobalState` rather than typed
  status snapshots, and display capability/profile selection remains coupled to
  application device configuration.
- **Target seam:** composition supplies an immutable display descriptor and
  concrete display adapter; screen policy renders portable feature snapshots.

## Relationships to other feature slices

| Related feature | Relationship |
|---|---|
| [Settings validation and persistence](../../configuration/settings-persistence/SPEC.md) | Owns timeout/rotation validation and schema agreement. |
| [Factory self-test lifecycle](../../manufacturing/factory-self-test/SPEC.md) | Self-test messages are pre-carousel safety/diagnostic state. |
| [Feature ownership and dependency boundaries](../../architecture/feature-ownership/SPEC.md) | Separates screen policy, status snapshots, display adapters, and composition. |

## Verification approach

- Unit-test a pure sleep/activity decision table and timer scheduling; use
  authorized devices for supported resolutions and physical wake/orientation.
- **Exact revision:** `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f`
- **Evidence classes:** source/schema reconciliation only in this change.

## Constraint provenance

- **Current repository evidence:** Startup-only layout, activity triggers,
  carousel-aware timeout, and separated one-second timer are present.
- **Historical review evidence:** [PR #525](https://github.com/bitaxeorg/ESP-Miner/pull/525) (mutatrum, merged; relevant threads resolved/outdated) established carousel-only sleep, button activity, and no arbitrary 60-minute cap. [PR #937](https://github.com/bitaxeorg/ESP-Miner/pull/937) (mutatrum, merged) established startup-only layout selection, resolution-specific layout freedom, and slower expensive refresh.
- **Disposition:** Accepted and corroborated by current code.
