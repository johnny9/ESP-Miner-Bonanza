# Stratum session and message safety — design

## Components and responsibilities

| Component | Responsibility | Implementation pointer |
|---|---|---|
| Shared socket | Resolution, address selection, timeouts, TCP_NODELAY | `components/stratum/stratum_socket.c` |
| V1 API/task | JSON framing/parsing, request IDs, setup order, ownership | `components/stratum/stratum_api.c`, `main/tasks/stratum_v1_task.c` |
| V2 protocol/Noise/task | Binary bounds, crypto transport, channel/session, buffers | `components/stratum_v2/`, `main/tasks/stratum_v2_task.c` |
| Protocol-to-work adapter | Convert validated V1/V2 job values to neutral mining work | Target extraction from `components/stratum/mining_template.c` |
| Client lifecycle | Select session and receive explicit setup/failure/exit events | `main/tasks/protocol_coordinator.c`, `main/tasks/protocol_coordinator.h`, `main/system.c`, `main/system.h` |

## Authority and dependency direction

| Fact or state | Owner | Consumers | Forbidden duplicate |
|---|---|---|---|
| Socket resolution/options | Shared socket component | V1 and V2 tasks | Divergent task-local socket policy |
| Noise cryptographic state | V2 Noise component/session | V2 transport task | Mining parser owning crypto buffers |
| Request/sequence timing | Active protocol session | Telemetry | Arrival-order latency guess |
| Parsed protocol-job memory | Active protocol session until explicit handoff | Protocol-to-work adapter | Retained pointer with ambiguous free owner |
| Mining capability snapshot | Mining composition/service | Protocol negotiation | ASIC queries or board-model checks in Stratum |
| Protocol/channel public value | NVS/OpenAPI string enum | Coordinator/tasks/AxeOS | Numeric/string drift or magic labels |

## Interfaces and cross-layer matrix

| Surface | Required reconciliation |
|---|---|
| Transport | Resolve, connect, timeout, no-delay, close/error distinction |
| V1/V2 protocol | Setup order, field/count bounds, status, reconnect reset |
| Mining service | Owned neutral work-source and share-sink handoff |
| OpenAPI/AxeOS | Consistent string protocol/channel fields and complete schema |
| NVS | Valid enum strings, certificate/key ownership, fallback values |
| Tests | Parser corpus, maximums, allocation, timing correlation, reconnect |

## Required and forbidden behavior

- Bound before copy or allocation; theoretical protocol maxima may be reduced to
  resource-safe device maxima when documented and reported precisely.
- Check every crypto, allocation, queue, and transport result.
- Send setup messages only in protocol-valid order.
- Keep unrelated refactors/network detection out of protocol feature patches.
- Do not log secrets, passwords, private keys, or full sensitive certificates.
- Do not include ASIC, board configuration, or global application-state headers
  in protocol/session components.

## Failure, ownership, and lifecycle boundaries

- Each session owns transport, parser buffers, Noise context, pending jobs, and
  timing slots until explicit handoff or close.
- Close destroys transport/Noise, clears queued jobs, and invalidates retained
  session state before retry.
- Blocking receive is bounded by socket timeout, not only an outer timer that
  cannot interrupt a blocked call.
- Allocation failure aborts setup and frees every already-owned buffer.

## Compatibility and resources

- V1/V2 remain separate public protocols behind one selection/coordinator
  contract; field names and values remain string-stable across API generations.
- `SV2_MAX_FRAME_SIZE` is 8192 bytes for current ESP resource constraints;
  increasing it requires memory, PSRAM-absence, fragmentation, and soak evidence.

## Current implementation boundary

- **Conforming:** parser, socket, and V2 Noise responsibilities are mostly
  componentized and retain protocol-specific ownership.
- **Violations:** main V1/V2 tasks include `asic.h`; V2 includes
  `device_config.h`; `components/stratum/stratum_api.h` mixes protocol services
  with ESP transport concerns; the neutral mining type currently lives under
  the Stratum component.
- **Target seam:** protocol cores depend on an injected transport port and
  immutable capability snapshot, then exchange neutral work/share values with
  the mining service. ESP transport and task lifecycle remain adapters.

## Relationships to other feature slices

| Related feature | Relationship |
|---|---|
| [Stratum client lifecycle and pool failover](../stratum-client-lifecycle/SPEC.md) | Owns pool selection, protocol-task lifetime, work handoff, failover, and recovery. |
| [Mining work and result pipeline](../work-pipeline/SPEC.md) | Owns neutral work after the protocol adapter handoff and returns share candidates by source identity. |
| [Feature ownership and dependency boundaries](../../architecture/feature-ownership/SPEC.md) | Defines the inward dependency direction and adapter boundary. |
| [Best-share scoreboard](../../observability/share-scoreboard/SPEC.md) | Receives bounded validated share metadata. |
| [Settings validation and persistence](../../configuration/settings-persistence/SPEC.md) | Owns protocol/channel/certificate setting domains and migrations. |

## Verification approach

- Run component tests and QEMU malformed-input cases at the exact SHA; add a
  mock transport for timeout/reconnect and use authorized mining soak when timing
  or failover behavior changes.
- **Exact revision:** `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f`
- **Evidence classes:** source/test-surface reconciliation only in this change.

## Constraint provenance

- **Current repository evidence:** Authorization ordering, ID/sequence timing,
  shared socket policy, bounds, TLS null check, and 8 KiB PSRAM buffers are
  present. V2 destroys transport/Noise on close; current V1 detaches and closes
  the transport without destroying it, so complete cleanup remains unchecked.
- **Historical review evidence:** Merged feedback from [#913](https://github.com/bitaxeorg/ESP-Miner/pull/913), [#1063](https://github.com/bitaxeorg/ESP-Miner/pull/1063), [#1292](https://github.com/bitaxeorg/ESP-Miner/pull/1292), [#1391](https://github.com/bitaxeorg/ESP-Miner/pull/1391), [#1413](https://github.com/bitaxeorg/ESP-Miner/pull/1413), [#1553](https://github.com/bitaxeorg/ESP-Miner/pull/1553), [#1722](https://github.com/bitaxeorg/ESP-Miner/pull/1722), and [#1783](https://github.com/bitaxeorg/ESP-Miner/pull/1783) by WantClue/mutatrum established ordering, correlation, timeout, bounds, TLS, responsibility, socket reuse, and resource-safe frame constraints. PR #717 also supports focused scope and generalized pool settings.
- **Disposition:** Accepted and corroborated. Open PR #1854 and unresolved local-work feedback are not promoted into this contract.
