# ESP-Miner review-derived engineering preferences

## Purpose

This guide turns recurring upstream ESP-Miner review feedback into operational
checks for agents, authors, and reviewers. It complements feature specs; it is
not a substitute for current code, tests, public schemas, hardware evidence, or
an explicit maintainer decision.

Apply the shared rules by default. The reviewer lenses near the end explain
where individual maintainers historically concentrated their attention; do not
optimize for a person, infer current ownership, or weaken a rule because a
specific reviewer is absent.

## Evidence and limits

The source analysis covered all 989 `bitaxeorg/ESP-Miner` PRs created from
2024-08-10 through 2026-08-10: 719 merged, 191 closed without merging, and 79
open at the cutoff. It included 1,855 submitted review records, 654 inline
threads, 1,212 inline comments, 2,417 conversation comments, and every PR diff.

The findings are observational. Approval and thread resolution are late
workflow signals, not causal estimates. Theme classification is directional,
and recent security-heavy reviews are not representative of every historical
maintainer or feature area. Re-check current repository policy and code before
applying a historical convention.

The raw corpus is intentionally not vendored: it is about 60 MiB, changes over
time, and contains discussion that agents do not need in routine context. This
document carries the compact, reviewable policy derived from it.

## Shared engineering rules

### Authority and single source of truth

- Identify the authoritative owner of every hardware fact, persistent value,
  runtime state, and public identifier.
- Query or consume the authority instead of maintaining a shadow flag, copied
  state, duplicated startup path, or parallel serialization path.
- Keep human-readable labels presentation-only. Use stable IDs, enums, typed
  fields, or protocol-safe names for persistence, queries, and message identity.
- Use exact storage and schema types. A key or type change requires an explicit
  compatibility and migration decision.

### Dependency direction and responsibility

- Hardware-model facts belong in board, ASIC, power, thermal, or configuration
  layers, not AxeOS and not generic runtime tasks.
- Firmware owns device behavior and validation. AxeOS renders and controls the
  typed public contract without re-deriving hardware semantics.
- Controllers coordinate state and lifecycle. Tasks and components own their
  domain-specific retrieval, logging, transitions, and cleanup.
- Reusable components must not depend upward on application-private `main/`
  headers or state when a lower-level interface can express the dependency.
- Prefer an existing component API or lifecycle path to a near-duplicate helper.

### Feature ownership and portability

- Apply the layered model in
  [feature ownership and dependency boundaries](architecture/feature-ownership/SPEC.md):
  portable domain values → feature policy → ports/facades → concrete adapters →
  composition root → presentation.
- `GlobalState` may hold composed application objects and snapshots, but it is
  not a public interface for reusable components or feature policy.
- Feature policy may depend on immutable profiles/snapshots and narrow injected
  ports. It must not include concrete board, ASIC-backend, regulator, bridge,
  ESP-IDF, protocol-transport, or persistence implementations.
- Concrete adapters may depend on platform and driver APIs. Select them once at
  composition; do not spread board/model/configuration branches across feature
  tasks.
- A feature is incomplete when it works only because another feature's parser,
  queue, global flag, task, or concrete backend is used as an implicit service.
  Define the missing neutral contract and move policy to its real owner.
- During review, name the exact current modules that conform or leak and the
  target owner, port, adapter, and composition change. “Use existing APIs” is
  not an adequate boundary description.

### Cross-layer contract completeness

For an API, WebSocket, configuration, NVS, or device-state change, explicitly
mark every row applicable or not applicable:

| Surface | Required reconciliation |
|---|---|
| Firmware authority | State owner, validation, serialization, and failure behavior |
| OpenAPI/protocol | Field type, nullability, units, bounds, compatibility, and generated name |
| Generated/client code | Regenerated output and typed consumer updates |
| AxeOS service/model | Runtime mapping, default behavior, and error handling |
| Mock/development data | Structurally valid values and behavior parity |
| UI | Presentation, controls, loading/error states, and no hardware re-derivation |
| Persistence | Default, migration, rollback, and old-firmware/old-client behavior |
| Tests | Focused backend, schema, frontend, regression, and hardware evidence as applicable |

Do not call a locally correct change complete when a downstream consumer or
contract surface is stale.

### Correctness, ownership, and failure paths

- Validate untrusted types, encodings, widths, lengths, counts, and bounds
  before parsing, indexing, comparing, or copying.
- State who owns each allocation, buffer, callback context, task argument, and
  retained pointer; state when ownership ends and cleanup occurs.
- Do not retain stack-backed data beyond its lifetime or assume an allocation,
  task creation, queue operation, or ESP-IDF call succeeded.
- Keep event-loop callbacks nonblocking. Move potentially slow DNS, network,
  storage, or hardware work into an appropriate task or asynchronous flow.
- Reset retained parser, transport, and session state at reconnect, close, and
  ownership-transfer boundaries.
- Fail closed for safety, protocol, configuration, and hardware-readiness
  violations, while preserving an observable recovery path.

### Scope and reviewability

- Prefer the smallest coherent patch. Separate unrelated cleanup, generated
  artifacts, dependency churn, or independently testable behavior.
- Record why a broad change must be atomic. Large surface area is a review and
  merge risk, especially across ASIC, NVS/configuration, build, and HTTP/API
  boundaries.
- Do not commit binaries, downloaded artifacts, dependency trees, raw logs, or
  extra patch files as implementation evidence.
- Resolve actionable review threads or explain the durable reason a requested
  change should not be made. A changes-requested review normally defines a
  repair path rather than a terminal outcome.

## Verification expectations

Choose verification from the changed risk and boundary, not from a fixed
ritual. Record commands and results actually observed.

| Change | Expected evidence |
|---|---|
| Pure internal refactor | Focused unit/regression tests and relevant build |
| Parser, transport, or untrusted input | Boundary, malformed, short, long, reconnect, and ownership regressions; QEMU when supported |
| API/configuration | Backend validation, OpenAPI generation, mock parity, frontend tests, and compatibility/migration checks |
| AxeOS behavior | Focused component/service tests plus production build when warranted |
| ASIC, power, thermal, fan, display, or board behavior | Unit/simulation evidence plus explicitly authorized relevant-board or mining/soak validation when required |
| Build/release/artifact flow | CI-equivalent command, provenance, content/size checks, and rollback/recovery evidence |

For security-sensitive or high-risk changes, identify the exact reviewed SHA
and distinguish unit, frontend, build, simulation/QEMU, manual device, mining,
soak, and HIL evidence. QEMU is a strong recent practice, particularly for
parser and network boundaries, but the history does not support pretending it
was uniformly required for every past change.

## Naming and style decisions

- Follow repository formatters and nearby conventions; do not create a second
  style system.
- Prefer stable semantic names over display text or implementation accidents.
- Put reusable OpenAPI types in `components.schemas` when it improves generated
  names and reuse.
- Match established schematic and hardware terminology unless a deliberate,
  repository-wide migration is in scope.
- Prefer private `static` implementation state, `const` where ownership allows,
  and explicit enums/types over mutable globals and magic strings.
- Prefer direct control flow and early returns over redundant branches.
- Keep build commands and file-selection logic centralized rather than copied.

## Reviewer lenses

These lenses help an agent perform a broad self-review. They do not assign
current ownership or override a maintainer's present feedback.

| Historical reviewer | Recurrent lens |
|---|---|
| `mutatrum` | Simplification, task responsibility, API/config contracts, hardware/UI separation, naming |
| `WantClue` | Pointer lifetime, allocation and bounds checks, task/event-loop failure paths, concrete safety fixes |
| `johnny9` | Exact-head adversarial review, input and session boundaries, QEMU regressions, full build evidence |
| `0xf0xx0` | OpenAPI schema organization, generated names, UX completeness, build reuse, soak behavior |
| `duckaxe` | OpenAPI/mock/UI propagation, missing downstream consumers, local simplification |
| `eandersson` | C resource and HTTP semantics, dependency direction, dependency versions, valid mocks |
| `skot` | Hardware abstraction, model-independent generic tasks, centralized nominal hardware data |
| `benjamin-wilson` | Framework source of truth instead of fallible shadow state |
| `Georges760` | Real mining behavior and state propagation for hardware-sensitive changes |

## Merge-readiness signals

Historical closed-PR associations provide useful workflow warnings:

- Collaborator approval: 96.8% merged, versus 46.4% without approval.
- All inline threads resolved: 92.9% merged; at least one unresolved: 77.7%.
- Changes requested: 80.0% merged, close to the 79.0% closed-PR baseline.
- One-file PRs: 82.9% merged; 51-plus-file PRs: 42.9%.
- Diffs over 100 KiB: 51.2% merged, versus 80.3% for smaller diffs.

Use approval and thread state as readiness signals. Use patch breadth as the
earlier, author-controllable signal. Do not infer that merely mentioning tests
or attracting comments changes merge probability; difficult work attracts both.

## Agent self-review before handoff

- [ ] The authoritative owner of every changed fact and state value is named.
- [ ] Dependency direction and component/task/controller responsibilities are
  preserved.
- [ ] Feature policy uses portable contracts and injected ports; concrete
  board/driver/ESP dependencies and backend selection remain in adapters and
  composition, not `GlobalState`-based module interfaces.
- [ ] Every applicable cross-layer contract surface is reconciled.
- [ ] Untrusted inputs, bounds, ownership, cleanup, failure paths, and session
  resets are covered.
- [ ] The patch is the smallest coherent unit and contains no stray artifacts.
- [ ] Focused regressions and proportional build/simulation/hardware evidence
  are recorded without overstating what ran.
- [ ] Compatibility, migration, rollback, and recovery are explicit where
  observable or persistent behavior changes.
- [ ] Review threads and intentional non-changes have a clear disposition.

## Changelog

- 2026-08-10: Added the initial policy derived from the two-year upstream PR
  review corpus.
