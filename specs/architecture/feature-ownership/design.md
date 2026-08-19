# Feature ownership and dependency direction — design

## Components and responsibilities

| Component | Responsibility | Implementation pointer |
|---|---|---|
| Portable domain contracts | Neutral work, results, capabilities, commands, events, snapshots, and statuses | Target: `components/mining/` and portable headers under `components/asic/include/` |
| Feature policy services | Own feature state machines and decisions | Current examples: `main/self_test/self_test.c`, `main/tasks/protocol_coordinator.c` |
| Ports/facades | Describe operations required by a feature without selecting a backend | Target contract modules: `components/asic/include/`, `components/mining/`, plus portable thermal, power, transport, storage, clock, and status port headers |
| Concrete adapters/backends | Translate ports to BM/BZM, board, regulator, ESP-IDF, NVS, network, and display mechanisms | Current: `components/asic/bm*.c`, `components/asic/bzm*.c`, `main/power/`, `main/thermal/`, `main/tasks/stratum_v1_task.c`, `main/tasks/stratum_v2_task.c` |
| Composition root | Select board profile/backends, allocate contexts, wire ports, start/stop application services | Target extraction from `main/main.c`, `main/system.c`, `main/bzm_controller.c` |
| Presentation adapters | Serialize typed snapshots and render/control public contracts | `main/http_server/`, `main/http_server/axe-os/`, `main/screen.c` |

## Authority and dependency direction

| Fact or state | Owner | Consumers | Forbidden duplicate |
|---|---|---|---|
| Feature policy/phase | Feature service context | Adapters and presentation snapshots | Driver- or UI-owned shadow state machine |
| Static board facts and qualification limits | Immutable board/profile descriptor | Composition and feature policy | Board-ID branches in generic tasks |
| Hardware/protocol mechanics | Concrete backend/adapter | Port facade | Feature reaching into registers, UART frames, ESP handles, or protocol structs |
| Runtime measurement/health | Backend-produced validity-aware snapshot | Policy, telemetry, self-test | Consumer reading backend-private globals |
| Cross-feature identity | Portable handle with generation/purpose | Work/result/event consumers | Pointer identity or mutable global mode flag |
| Platform resource | ESP adapter that created it | Port operation | Domain service closing/freeing an ESP handle |

## Allowed dependency model

Dependencies point downward in this order; callbacks/events return upward only
through the same contract:

| Layer | May depend on | Must not depend on |
|---|---|---|
| Presentation/API | Feature snapshots and commands | Concrete board/driver APIs or re-derived hardware policy |
| Composition/application | Every public port/adapter needed for wiring | Backend-private headers outside construction/wiring |
| Feature policy | Portable contracts and injected ports | `GlobalState`, ESP-IDF, board IDs, concrete ASIC/power/thermal/Stratum adapters |
| Port/facade | Portable domain types | Application-private `main/` state or another feature implementation |
| Concrete adapter/backend | Its port, domain types, and required platform/driver APIs | Presentation or unrelated feature policy |
| Portable domain | Standard C and portable domain types | ESP-IDF, `main/`, drivers, protocols, storage, or UI |

`GlobalState` is application composition/storage, not a component API. New or
migrated public operations take an opaque instance context plus narrow values.

## Contract vocabulary

- **Descriptor/profile:** Immutable facts, limits, and optional capabilities;
  never a mutable runtime object or driver switch.
- **Port:** Operations a feature requires, including typed failure behavior.
- **Adapter/backend:** Concrete implementation of a port.
- **Command:** Owned request to change state, with one acknowledged outcome.
- **Event:** Immutable occurrence tagged with source, generation, purpose, and
  ownership/lifetime.
- **Snapshot:** Consistent read-only runtime state with validity and timestamp.
- **Composition root:** The only place that selects and wires concrete adapters.

## Current implementation boundary

| Current violation | Evidence | Target seam |
|---|---|---|
| ASIC component depends upward on application and Stratum | `components/asic/CMakeLists.txt` requires `stratum`/`stratum_v2` and adds private `main/` include paths; public ASIC headers include `global_state.h`/`mining.h` | Portable mining contract plus opaque ASIC instance/driver port |
| Self-test owns concrete hardware and protocol mechanics | `main/self_test/self_test.c` includes DS4432U, thermal, VCORE, reset, NVS, GPIO, PID, `GlobalState`, and parses/enqueues synthetic Stratum work | Pure self-test policy with qualification profile and injected diagnostic/power/thermal/status ports |
| Stratum tasks query ASIC and device configuration | `main/tasks/stratum_v1_task.c` and `stratum_v2_task.c` include `asic.h`; V2 also includes `device_config.h` | Mining capability snapshot and neutral work-source/share-sink ports |
| Result task is a cross-feature hub | `main/tasks/asic_result_task.c` calls ASIC, self-test, monitoring, scoreboard, and V1/V2 submission | Mining result service plus typed observers and protocol share sink |
| Generic safety paths contain BZM details | `main/thermal/thermal.c`, `main/power/vcore.c`, `asic_reset.c`, and `fan_controller_task.c` branch on Bonanza/BZM data | Board-selected thermal/power/reset adapters and immutable safety profile |
| Shared application struct defines module interfaces | `main/global_state.h` embeds protocol, ASIC store, device, power, self-test, display, transport, and telemetry state | Per-feature contexts and explicit composition-owned references |

## Migration model

1. **Characterize:** Record current behavior, ownership, exact dependencies, and
   failures; add tests before changing behavior.
2. **Introduce contracts:** Add standard-C neutral types, descriptors, statuses,
   and ports beside existing APIs without moving policy yet.
3. **Wrap backends:** Implement Bitmain, BZM, board, and ESP adapters behind the
   ports; keep old entry points as temporary compatibility wrappers.
4. **Move policy:** Convert one feature at a time to its context/ports and typed
   events; stop reading concrete state or calling concrete helpers.
5. **Compose once:** Move backend selection and lifecycle wiring to the
   composition root; generic tasks become thin scheduling adapters.
6. **Remove compatibility:** Delete upward includes, duplicate flags, board
   branches, and old wrappers after all consumers migrate.
7. **Prove:** Run architecture checks, host unit tests, integration/QEMU, and
   explicitly authorized affected-board/HIL evidence before marking complete.

Each step is an independently buildable/reviewable change. Do not combine the
entire migration with a feature behavior change unless atomic safety requires it.

## Required and forbidden behavior

- Missing required ports/capabilities fail startup before side effects.
- Optional capabilities are explicit data, never inferred from a NULL callback,
  board name, or failed operation.
- Feature services own policy; drivers own mechanics; profiles own facts;
  adapters own resources; composition owns wiring.
- Do not create a generic “hardware manager” that simply relocates switches and
  `GlobalState` access behind one large API.
- Do not put protocol routing metadata into ASIC backend-private packets or
  hardware register details into Stratum/self-test contracts.

## Relationships to other feature slices

| Related feature | Relationship |
|---|---|
| [ASIC driver contract](../../asic/driver-contract/SPEC.md) | Defines the opaque, capability-driven hardware facade and backend boundary. |
| [Protocol-neutral mining work pipeline](../../mining/work-pipeline/SPEC.md) | Defines shared work/result/events between Stratum, ASIC, self-test, and observers. |
| [Factory self-test lifecycle](../../manufacturing/factory-self-test/SPEC.md) | Migrates concrete hardware/protocol calls to profiles and diagnostic ports. |
| [Stratum client lifecycle](../../mining/stratum-client-lifecycle/SPEC.md) | Migrates ASIC queries and result-task coupling to mining ports. |
| [Thermal, fan, and overheat control](../../device-safety/thermal-fan-overheat/SPEC.md) | Migrates BZM/board branches to selected safety adapters. |

## Verification approach

- Add include/CMake dependency checks, compile portable contracts in a host C
  target, and unit-test feature services with fake ports before backend/HIL work.
- **Exact revision:** `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f`
- **Evidence classes:** source and specification reconciliation only.

## Constraint provenance

- **Current repository evidence:** The violations table is directly
  corroborated by current includes, CMake dependencies, shared state, and calls.
- **Historical review evidence:** Merged review constraints already require
  capability-driven ASIC dispatch, board facts outside generic tasks, controller
  coordination rather than domain ownership, reusable components without upward
  dependencies, and focused migrations; affected feature specs retain the exact
  PR/reviewer/thread provenance.
- **Disposition:** This model consolidates accepted directions and current
  contradictions. It does not claim the target modules are implemented.
