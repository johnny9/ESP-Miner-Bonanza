---
name: specs
description: Create, update, reconcile, and review durable ESP-Miner feature specifications under specs/. Use for new features and behavior, API, configuration, hardware, OTA, safety, recovery, compatibility, or resource changes.
---

# ESP-Miner specifications

Maintain durable intent without duplicating implementation. Read `AGENTS.md`,
`specs/README.md`, and `specs/INDEX.md` before changing a feature contract.
Never assume a local device, serial port, secret, network, ignored profile, or
hardware-in-the-loop authorization.

## Core contract

1. Locate the relevant feature in `specs/INDEX.md` and read all companion
   documents, especially acceptance and risks.
2. Treat specs as intent and current code, tests, builds, and measurements as
   implementation evidence. Report contradictions rather than silently picking
   one source.
3. For new work, choose a stable domain-based area and feature slug, copy
   `specs/_template/`, and add one index row.
4. Update implementation and affected specifications together when observable
   behavior, REST/WebSocket schema, configuration, NVS state, hardware
   behavior, flashing/OTA, safety, recovery, compatibility, timing, memory,
   power, or image-size constraints change.
5. Do not require a spec update for formatting, dependency-only work,
   test-only cleanup, internal renames, or behavior-preserving refactors.
6. Apply `specs/REVIEW-PREFERENCES.md`: name each source of truth, preserve
   dependency direction, reconcile every applicable cross-layer consumer, make
   ownership and failure boundaries explicit, and keep the change reviewable.
7. For feature ownership or module changes, apply
   `specs/architecture/feature-ownership/SPEC.md`. Treat `GlobalState` as
   composition/storage, not a component API; feature policy consumes portable
   values and injected ports, while concrete board/driver/ESP implementations
   remain adapters selected at composition.

## Feature-document rules

- `SPEC.md` has a stable ID, lifecycle, owner, reconciliation date, links, and
  a dated changelog.
- `intent.md` covers the problem, desired outcome, primary/failure flows, and
  non-goals.
- `acceptance.md` contains independently verifiable criteria. Mark an item
  complete only with current evidence; distinguish unit, frontend, build,
  simulation, and HIL evidence.
- `design.md` records durable responsibilities, interfaces, required/forbidden
  behavior, flow, recovery, compatibility, resources, and relationships.
- `design.md` names exact current implementation modules (not “existing APIs”),
  the target owner for each responsibility, dependency direction, and a
  `Current implementation boundary` section listing conforming parts, concrete
  leaks, and the target port/adapter/composition seam.
- `design.md` records constraint provenance when historical review evidence is
  used. Include PR, reviewer, merge/thread disposition, and whether current
  code corroborates or contradicts the requested behavior; do not promote an
  open or unresolved suggestion without saying so.
- `risks.md` records scope, assumptions, open questions, failure modes,
  security/privacy/safety, resource risks, and rollout/rollback.
- Review the completed documents using the agent checklist in
  `specs/REVIEW-PREFERENCES.md`; historical reviewer lenses are coverage aids,
  not current ownership assignments or permission to optimize for a person.

## ESP-Miner-specific cautions

- API changes must reconcile `main/http_server/openapi.yaml`, generated AxeOS
  API code, service mocks, backend behavior, and tests.
- Hardware writes, configuration changes, flashing, OTA, and unit-test flashes
  need explicit user authorization, verified board/target/artifact identity,
  and a recovery path. A unit-test flash replaces the normal firmware.
- Use current project commands from `AGENTS.md`; do not claim builds, tests,
  device checks, or HIL that did not run.
- Do not turn a spec into a class catalog or copy secrets, device coordinates,
  raw logs, or local paths into it.

## Reconcile and finish

1. Compare every affected acceptance criterion with current code and evidence.
2. Reconcile the source-of-truth declaration and cross-layer change matrix with
   implementation, schema, generated code, mocks, UI, persistence, and tests.
3. Reconcile module ownership as well as behavior: identify concrete upward or
   sibling dependencies, choose the portable contract owner, and record which
   adapters and composition code must replace them.
4. Update lifecycle, last-reconciled date, evidence, and changelog.
5. Confirm each live feature appears once in `specs/INDEX.md`, links resolve,
   and placeholders remain only under `specs/_template/`.
6. Apply [specs/MAINTENANCE.md](../../../specs/MAINTENANCE.md), run
   `python3 tools/validate_specs.py` and `git diff --check`, then run relevant
   project verification or state its precise limitation.
