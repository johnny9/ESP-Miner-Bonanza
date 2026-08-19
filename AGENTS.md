# ESP-Miner agent guide

## Purpose and orientation

ESP-Miner is ESP-IDF firmware for Bitaxe Bitcoin ASIC miners and includes the
Angular AxeOS web interface. Start with [readme.md](readme.md) for supported
hardware and user-facing behavior. For any non-trivial work, read
[specs/OVERVIEW.md](specs/OVERVIEW.md), locate the relevant contract in
[specs/INDEX.md](specs/INDEX.md), and then read its companion documents.

## Working behavior

- Inspect the current code, tests, configuration contracts, and relevant specs
  before proposing or changing behavior.
- State material assumptions before changing a public API, hardware behavior,
  persistent configuration, compatibility promise, security boundary, or
  safety rule.
- Prefer the smallest coherent change; preserve unrelated work.
- For a defect, establish evidence and add a regression that fails before the
  fix when practical.
- Keep a short plan for multi-step work and include explicit verification.
- Never claim a test, build, flash, OTA, device run, or hardware-in-the-loop
  result that was not actually performed.

## Specifications

Use [.agents/skills/specs/SKILL.md](.agents/skills/specs/SKILL.md) for a new
feature, durable behavior change, reconciliation, or product/architecture
review. Specs record durable intent; code, tests, and measurements are
implementation evidence. Report contradictions rather than silently choosing
one source.

For feature or module-boundary work, also read
[feature ownership and dependency boundaries](specs/architecture/feature-ownership/SPEC.md).
Every affected design must name the current implementation modules, state
whether each boundary conforms or leaks, and name the target owner and seam.
`GlobalState` is composition/storage, not a reusable component interface.
Feature policy may depend on portable value types and injected ports; concrete
board, ASIC, driver, ESP-IDF, transport, and persistence implementations are
adapters selected by the composition root.

Update the affected feature specification in the same change when a change
alters observable behavior, the REST/WebSocket API, configuration or NVS
state, hardware interface, flashing/OTA procedure, safety/recovery rule,
compatibility, timing, memory, power, or image-size constraint. Formatting,
test-only cleanup, internal renames, and behavior-preserving refactors do not
need a spec update.

## Reviews

Before reviewing a pull request, patch, implementation, architecture proposal,
or feature specification, read and apply
[specs/REVIEW-PREFERENCES.md](specs/REVIEW-PREFERENCES.md). It converts two
years of upstream ESP-Miner review history into a repository-specific review
checklist. Use the shared rules to assess the change; reviewer profiles are
coverage lenses, not a reason to imitate an individual, tailor code to a person,
or bypass current requirements.

Review the exact change and current repository state. Separate actionable
findings from optional suggestions and positive acknowledgements. Do not infer
that a historical merge correlation makes a change correct, and do not submit
a GitHub review unless the user explicitly asks for that external action.

In particular:

- Name one authoritative owner for each hardware fact, state value, and public
  identifier. Consumers must not silently re-derive or shadow it.
- Preserve dependency direction: hardware facts stay in board, ASIC, power, or
  configuration layers; firmware owns device semantics; AxeOS presents typed
  API state; controllers coordinate while domain tasks own their work.
- Reject feature-policy dependencies on `GlobalState`, concrete board/driver
  headers, ESP-IDF APIs, or sibling feature internals. Prefer immutable
  profiles/snapshots and narrow ports, with backend selection performed once at
  composition.
- Treat a changed contract as a cross-layer change. Reconcile firmware,
  OpenAPI, generated client code, service mocks, UI consumers, persistence,
  compatibility, and tests wherever applicable.
- Make resource ownership, input bounds, failure behavior, task/callback
  lifetime, and transport/session reset boundaries explicit.
- Keep patches focused. Split unrelated or independently reviewable behavior
  unless atomicity requires it to move together.

## Source-of-truth order

- [readme.md](readme.md): supported products, user behavior, build orientation.
- [flashing.md](flashing.md): factory-image recovery guidance.
- [doc/unit_testing.md](doc/unit_testing.md): firmware unit-test procedure and
  its flashing warning.
- [main/http_server/openapi.yaml](main/http_server/openapi.yaml): public HTTP
  API contract; generated AxeOS client code follows from it.
- `specs/`: durable feature contracts and acceptance criteria.
- [specs/REVIEW-PREFERENCES.md](specs/REVIEW-PREFERENCES.md): evidence-derived
  architecture, review, scope, and verification defaults.
- Code and tests: current implementation evidence to reconcile with intent.

## Project boundaries and hardware safety

- Do not flash hardware, run OTA, alter a device configuration, or use a
  serial port unless the user explicitly authorizes the target and operation.
- A unit-test flash replaces normal firmware. Use a dedicated test device or
  preserve a recovery path as described in `doc/unit_testing.md`.
- Preserve rollback and recovery paths for configuration, firmware, and web
  assets. Treat board identity, target, image, and artifact provenance as
  required preconditions for destructive hardware work.
- Keep credentials and private network/device details out of source, logs,
  documentation, test fixtures, and commits.
- Bound untrusted network, serial, API, and uploaded-data handling. Do not
  retry a state-changing operation unless its idempotence is established.
- Changes to `openapi.yaml` require regenerated AxeOS API code and matching
  mock-data updates in `system.service.ts`.

## Coding-style standards

The repository configuration is authoritative. Do not introduce a competing
formatter or style guide.

### Firmware C/C++

- Format changed C/C++ with the repository [`.clang-format`](.clang-format):
  LLVM-derived style, four-space indentation, 132-column limit, middle pointer
  alignment, and braces on their own lines for declarations.
- Follow nearby component conventions for naming, include ordering, logging,
  ESP-IDF error handling, and `esp_err_t` ownership. Prefer existing component
  APIs over duplicate helpers.
- Keep headers self-contained, public declarations minimal, and implementation
  details private. Update component CMake registration and target tests when
  adding a component source, header, or unit-test suite.
- Make error paths explicit and fail safe. Check allocations and hardware
  readiness before use; do not hide a hardware, protocol, or configuration
  failure behind a best-effort success path.

### AxeOS Angular

- Follow `main/http_server/axe-os/.editorconfig`: UTF-8, final newline,
  trailing-whitespace cleanup, two-space TypeScript indentation, four-space
  HTML/SCSS indentation, and single-quoted TypeScript strings.
- Use the Angular and TypeScript patterns already present in the affected
  feature. Prefer functional providers such as `provideRouter([])` and
  `provideHttpClient()` in tests rather than deprecated testing modules.
- Keep API schema, generated client, service mocks, UI behavior, and focused
  tests synchronized. Do not manually edit generated API output as a substitute
  for changing `openapi.yaml` and running generation.

## Commands and verification

Source the ESP-IDF v5.5.3 environment before firmware commands. Use the
repository's actual commands; do not invent formatters, linters, hardware
commands, or deployment paths.

| Purpose | Command |
|---|---|
| Firmware build | `idf.py build` |
| Firmware unit-test build | `cd test && idf.py build` |
| AxeOS API generation | `cd main/http_server/axe-os && npm run generate:api` |
| AxeOS production build | `cd main/http_server/axe-os && npm run build` |
| AxeOS CI tests | `cd main/http_server/axe-os && npm run test:ci` |
| Factory configuration validation | `python3 tools/validate_bitaxe_1002_config.py` |
| Specification integrity | `python3 tools/validate_specs.py` |
| Whitespace validation | `git diff --check` |

Choose verification proportional to the change. Documentation-only work needs
the specification maintenance checks and `git diff --check`; it does not
authorize a build, network access, or device operation.

## Repository hygiene and done criteria

- Never commit credentials, local configuration, device coordinates, build
  output, downloaded firmware, captured logs, or generated dependency trees.
- Inspect staged and unstaged changes separately; stage only in-scope files.
- Commit and push only when explicitly authorized.
- Work is done when the requested behavior is implemented, affected durable
  specs and acceptance evidence agree, relevant verification has passed (or
  its limitation is precise), and unrelated files are untouched.
