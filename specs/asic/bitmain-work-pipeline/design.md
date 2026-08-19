# Bitmain ASIC backend — design

## Components and responsibilities

| Component | Responsibility | Implementation pointer |
|---|---|---|
| Composition/profile adapter | Select one ASIC model/count and official operating bounds | Current `main/device_config.c`; target immutable hardware profile at composition |
| Generic ASIC driver contract | Opaque lifecycle, capabilities, neutral work/results, diagnostics | [ASIC driver contract](../driver-contract/SPEC.md); target extraction from `components/asic/asic.c` and `asic_driver.c` |
| Neutral mining contract | Own work identity, provenance, execution purpose, and candidate routing | [Mining work and result pipeline](../../mining/work-pipeline/SPEC.md) |
| Bitmain builder | Convert neutral binary header material to common `bm_job` fields/midstates | `components/asic/bm_job_builder.c` |
| Model driver | Detect/configure chips, encode/send packets, and decode UART results | `components/asic/bm1397.c`, `bm1366.c`, `bm1368.c`, `bm1370.c` |
| Backend slot store | Associate bounded private hardware slots with neutral work handles | `components/asic/asic_job_store.c` pending separation from protocol metadata |
| Result normalization | Convert raw result to a neutral driver event/result | `components/asic/bm_result.c`; target split from `asic_result_handler.c` routing |
| Power control | Own voltage, thermal safety, and requested operating frequency | `main/tasks/power_management_task.c`, `components/asic/frequency_transition_bmXX.c` |

## Authority and dependency direction

| Fact or state | Owner | Consumers | Forbidden duplicate or upward dependency |
|---|---|---|---|
| Board ASIC model/count/limits | Immutable hardware profile at composition | ASIC facade, safety, status | Model guesses in Stratum or AxeOS |
| Driver operations and chip identity | ASIC driver table/model driver | Generic ASIC facade | Switch dispatch spread through main tasks |
| Rolling/work-refresh capability | ASIC capabilities | V1/V2 negotiation and work cadence | Protocol code checking `BM####` names |
| Neutral work and source provenance | Mining service | ASIC driver port and share sink | Protocol metadata owned by a model backend |
| Hardware slot to neutral work handle | Bitmain backend slot store | Model decoder | Unlocked parallel slot tables in each task |
| Voltage/safety target | Power management/device safety | ASIC frequency request and regulator | ASIC driver setting voltage policy |

## Interfaces and contracts

### Driver contract

- The selected Bitmain driver provides `init`, `process_work`, `send_work`,
  baud, version-mask, hash-frequency, and applicable nonce/register operations.
  Required operations are validated before the mining stack becomes usable.
- One configured board/chain contains one ASIC type. Mixed-model arrays or
  model values passed into shared helpers are not part of the contract.
- Shared Bitmain-family helpers stay in `components/asic`; generic Stratum and
  main mining tasks depend only on the facade and capabilities.
- The public facade takes an opaque per-instance device, immutable descriptor,
  purpose-aware start, reason-aware stop, neutral work/results, and diagnostic
  operations. It never takes `GlobalState`.

### Work and job-store contract

- The mining service validates and owns neutral work. The Bitmain builder
  consumes fixed-width binary fields and owns no protocol parsing, hex decoding,
  source-session metadata, or share routing.
- Sending work first associates a neutral handle with a private hardware slot;
  if that association fails, no packet is sent.
- The store is fixed-capacity, mutex-protected, initialized before any driver
  task, and invalidated on clean jobs, reconnect, reset, or mining stop.
- Bitmain compatibility slots are bounds-checked. Because hardware returns only
  a slot, reuse must avoid accepting a result from the prior assignment; tests
  define the model-specific safe reuse/flush behavior.

### Result contract

- Raw driver results are borrowed only until the next serialized poll and are
  immediately normalized to `asic_event_t`/`asic_result_t`.
- Register and share results remain distinct. Unknown registers, malformed UART
  frames, impossible indices, stale slots, invalid version bits, or absent
  metadata do not reach share submission/accounting as valid work.
- The mining service computes candidate validity/difficulty and routes a share
  through the source adapter retained in provenance. The Bitmain backend has no
  V1/V2 knowledge.

### Frequency and safety contract

- The common transition helper accepts a model frequency callback with a common
  signature; it does not switch on model.
- Device configuration bounds the officially supported frequency. PLL constraints
  shared by BM1366/BM1368/BM1370 remain aligned unless target evidence justifies
  a model exception.
- Power management is the guardian of operating frequency transitions and owns
  voltage, wind-down, pause, thermal, and overheat priority.

## Required and forbidden behavior

- Do not add Bitmain model checks to Stratum, `create_jobs_task`, result routing,
  power policy, or UI business logic when a driver/capability can own the fact.
- Do not send work when template construction, job-store cloning, required
  driver operation, or ASIC initialization fails.
- Do not change reset/serial/detection order, PLL rules, packet delay, or job-ID
  math without focused tests and affected hardware evidence.
- Preserve fractional frequency where supported and the established legacy
  integer NVS compatibility contract in the settings feature.

## Control and data flow

1. A work-source or diagnostic producer creates neutral mining work.
2. The mining service asks the generic facade to send it; capabilities determine
   refresh/rolling behavior without model knowledge.
3. Bitmain backend builds packet material, associates a private slot with the
   neutral handle, and sends its packet.
4. The mining service polls the facade, resolves provenance, validates a
   candidate, then independently notifies self-test/metrics/scoreboard and the
   appropriate source share sink.

## Failure and recovery

- Unknown model or missing required operation → initialization fails, no mining
  tasks start, and system status names the internal ASIC configuration failure.
- Zero/wrong chip detection or serial/reset failure → ASIC remains uninitialized;
  power follows the hardware-safe recovery path.
- Store allocation/clone failure → reject the work before UART send.
- Unknown/stale result → reject/account as local invalid where applicable; never
  submit against current work merely because the slot number matches.

## Compatibility and resources

- Supported current Bitmain identities are BM1397, BM1366, BM1368, and BM1370.
  New models add one driver/capability/config entry and focused vectors; protocol
  tasks should not require model-specific edits.
- The job store is bounded (currently 256 slots) and clones variable metadata;
  changes require heap/fragmentation and result-latency evidence.
- One ASIC result task serializes borrowed static model/event results. Parallel
  polling requires caller-owned results or another explicit lifetime design.

## Current implementation boundary

- **Conforming:** model tables/capabilities, common Bitmain builders, model
  drivers, and frequency callbacks already reduce chip-name switches.
- **Violations:** the ASIC component has direct Stratum build dependencies and
  private include paths into `main`; public headers expose application state and
  protocol-owned types; the job store and result handler retain protocol
  metadata and route V1/V2 shares.
- **Target seam:** make Bitmain a private implementation of the opaque ASIC
  driver contract. Move neutral work/provenance/result policy to the mining
  service and select the backend once in the composition root.

## Relationships to other feature slices

| Related feature | Relationship |
|---|---|
| [ASIC driver contract](../driver-contract/SPEC.md) | Defines the portable opaque facade that this Bitmain backend implements. |
| [Mining work and result pipeline](../../mining/work-pipeline/SPEC.md) | Owns neutral work, provenance, result validation, observers, and share routing. |
| [Feature ownership and dependency boundaries](../../architecture/feature-ownership/SPEC.md) | Keeps the concrete backend below reusable policy and composition. |
| [Stratum session and message safety](../../mining/stratum-session-safety/SPEC.md) | Validates untrusted protocol input before the neutral work boundary. |
| [Thermal, fan, and overheat control](../../device-safety/thermal-fan-overheat/SPEC.md) | Owns voltage, thermal priority, safe stop, and power recovery. |
| [Settings validation and persistence](../../configuration/settings-persistence/SPEC.md) | Owns frequency bounds, float/legacy persistence, and migration. |

## Verification approach

- Use golden template→packet and raw-result→event vectors for every model;
  failure-inject store/driver initialization; then verify reset/detect/frequency,
  work cadence, slot reuse, and accepted shares on named boards/ASIC counts.
- **Exact revision:** `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f`
- **Evidence classes:** source/test-surface reconciliation only in this change.

## Constraint provenance

- **Current repository evidence:** The driver table, capabilities, neutral
  template, common Bitmain builder, mutexed owned job store, normalized events,
  and common result routing are present. Job-store init failure is not fail-closed.
- **Historical review evidence:** Merged [#747](https://github.com/bitaxeorg/ESP-Miner/pull/747) resolved mutatrum's callback-based frequency helper, single-ASIC-type board, and no model logic in shared layers; WantClue agreed with the callback direction. Merged [#745](https://github.com/bitaxeorg/ESP-Miner/pull/745) corroborated chip-ID/CRC validation. Merged [#986](https://github.com/bitaxeorg/ESP-Miner/pull/986) left an explicit reset-before-serial testing concern. Merged [#1051](https://github.com/bitaxeorg/ESP-Miner/pull/1051) resolved aligned PLL constraints and persistence compatibility. Merged [#1321](https://github.com/bitaxeorg/ESP-Miner/pull/1321) moved toward binary work construction but retained an unresolved endianness-test concern. Merged [#1557](https://github.com/bitaxeorg/ESP-Miner/pull/1557) resolved official device frequency bounds. Merged [#1608](https://github.com/bitaxeorg/ESP-Miner/pull/1608) places frequency guardianship/wind-down in power management. Merged [#1779](https://github.com/bitaxeorg/ESP-Miner/pull/1779) resolved shared V1/V2 difficulty conversion. Merged [#478](https://github.com/bitaxeorg/ESP-Miner/pull/478) established safe extranonce rollover, while merged [#1168](https://github.com/bitaxeorg/ESP-Miner/pull/1168) exposed the need for one shared maximum.
- **Disposition:** Resolved merged constraints and current corroborated boundaries
  are normative. Unresolved model-delay/difficulty ideas in open PRs #1807 and
  #1849, and the outdated unified-`mining_notify` suggestion in #1553, are not.
