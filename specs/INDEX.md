# Specification index

Every feature-level `SPEC.md` in this repository appears exactly once in the
table below. Create new entries from [_template/](./_template/) as durable
behavior is changed or reviewed.

| Area | Feature | Lifecycle | Link | Summary |
|---|---|---|---|---|
| `architecture` | Feature ownership and dependency direction | `implementing` | [SPEC.md](architecture/feature-ownership/SPEC.md) | Portable contracts, explicit ports, concrete adapters, one composition root, and staged dependency cleanup. |
| `asic` | ASIC driver contract | `implementing` | [SPEC.md](asic/driver-contract/SPEC.md) | Opaque capability-driven lifecycle, work, diagnostics, telemetry, and backend isolation. |
| `asic` | Bitmain ASIC backend | `implementing` | [SPEC.md](asic/bitmain-work-pipeline/SPEC.md) | Concrete BM1397/BM1366/BM1368/BM1370 mechanics behind the generic ASIC contract. |
| `axeos` | Dashboard statistics selection | `supported` | [SPEC.md](axeos/dashboard-statistics/SPEC.md) | Browser-local chart choices and stable selective telemetry columns. |
| `axeos` | Swarm mixed-version compatibility | `supported` | [SPEC.md](axeos/swarm-compatibility/SPEC.md) | Tolerant remote-device API, timeout, identity, and missing-field boundary. |
| `configuration` | Settings validation and persistence | `implementing` | [SPEC.md](configuration/settings-persistence/SPEC.md) | Atomic validation, exact NVS representations, and explicit migrations. |
| `device-safety` | Thermal, fan, and overheat control | `implementing` | [SPEC.md](device-safety/thermal-fan-overheat/SPEC.md) | Hardware-owned sensors, bounded fan control, and fail-safe mining lifecycle. |
| `device-ui` | On-device display lifecycle | `supported` | [SPEC.md](device-ui/display-lifecycle/SPEC.md) | Startup layout, carousel-aware sleep, activity, and bounded refresh cadence. |
| `manufacturing` | Factory self-test lifecycle | `implementing` | [SPEC.md](manufacturing/factory-self-test/SPEC.md) | Portable qualification policy over injected safety, diagnostic-mining, persistence, and platform ports. |
| `mining` | Stratum client lifecycle and pool failover | `implementing` | [SPEC.md](mining/stratum-client-lifecycle/SPEC.md) | Pool/session ownership, work-source and share-sink ports, bounded failover, pause, and recovery. |
| `mining` | Stratum session and message safety | `implementing` | [SPEC.md](mining/stratum-session-safety/SPEC.md) | Bounded V1/V2 codec/session adapters with no ASIC or board authority. |
| `mining` | Protocol-neutral mining work pipeline | `implementing` | [SPEC.md](mining/work-pipeline/SPEC.md) | Owned work/provenance, ASIC results, share candidates, diagnostics, and observer fan-out. |
| `networking` | Hostname, mDNS, and Origin safety | `implementing` | [SPEC.md](networking/hostname-mdns-origin/SPEC.md) | Separate persistent identity, runtime discovery, and browser-origin trust. |
| `observability` | Best-share scoreboard | `implementing` | [SPEC.md](observability/share-scoreboard/SPEC.md) | Mutex-protected bounded shares, NVS slots, API schema, and UI timeout. |

Allowed lifecycle values: `proposed`, `implementing`, `supported`,
`deprecated`, and `retired`.
