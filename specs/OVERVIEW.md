# ESP-Miner — overview

## Purpose

ESP-Miner provides ESP32-S3 firmware and the AxeOS web interface for Bitaxe
Bitcoin ASIC miners. It coordinates ASIC work, pool connectivity, settings,
telemetry, local display behavior, HTTP/WebSocket control, firmware/web-asset
updates, and board-specific safety behavior.

## System boundaries

| Surface | Owns | Does not own |
|---|---|---|
| Firmware | Device lifecycle, ASIC/power/fan control, networking, persistent settings, API implementation, and update handling | Browser UI presentation or external pool operation |
| AxeOS | Browser-based configuration, status, control, and generated API use | Firmware authority, persistent device-state semantics, or bypassing backend validation |
| OpenAPI contract | Public HTTP operation and payload shape | Undocumented backend-only implementation details |
| Build and release workflow | Reproducible firmware, web assets, tests, and artifacts | Authorization to flash or alter a physical device |
| Portable domain contracts | Neutral work, result, lifecycle, profile, and status value types | ESP-IDF, concrete boards/drivers, transports, persistence, or UI |
| Feature policy | Decisions and state machines for mining, safety, self-test, networking, and presentation state | Selecting or directly calling concrete hardware/platform implementations |
| Ports and concrete adapters | Narrow capability interfaces and their ASIC, board, ESP, protocol, and storage implementations | Cross-feature policy or global application authority |
| Composition root | Select implementations, build immutable profiles, allocate shared storage, and wire tasks | Reusable domain behavior or feature decisions |

## Cross-cutting constraints

- Firmware must protect device operation, board identity, configuration
  integrity, and recovery from failed updates or changed settings.
- Client and firmware changes that alter the public API require coordinated
  schema, generated-client, mock, implementation, and test updates.
- Resource-constrained behavior must explicitly consider timing, memory, power,
  flash/image size, networking, and recovery as applicable.
- Hardware writes, flashing, OTA, and destructive tests require explicit target
  authorization and a documented recovery path.
- Secrets and private device information remain outside version control and
  user-visible API/log payloads.
- `GlobalState` is composition/storage, not a reusable component interface.
  Feature policy receives immutable profiles/snapshots and injected ports;
  concrete backend selection occurs once in the composition root.

## Developer orientation

- [AGENTS.md](../AGENTS.md) — working rules, source-of-truth order, and style.
- [readme.md](../readme.md) — products, features, developer setup, and API use.
- [doc/unit_testing.md](../doc/unit_testing.md) — firmware test process.
- [INDEX.md](INDEX.md) — complete feature directory.
- [STORY-MAP.md](STORY-MAP.md) — outcome-oriented navigation.
- [MAINTENANCE.md](MAINTENANCE.md) — specification checks.
- [REVIEW-PREFERENCES.md](REVIEW-PREFERENCES.md) — evidence-derived
  engineering and review defaults.
- [Feature ownership and dependency boundaries](architecture/feature-ownership/SPEC.md)
  — repository-wide module ownership and portability model.

## Changelog

- 2026-08-11: Defined portable policy, port/adapter, and composition boundaries.
- 2026-08-10: Established the feature-specification framework.
