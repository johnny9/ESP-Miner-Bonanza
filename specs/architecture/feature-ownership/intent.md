# Feature ownership and dependency direction — intent

## Problem

Feature policy currently crosses implementation boundaries through
`GlobalState`, `DeviceConfig`, ESP-IDF handles, concrete Bitmain/BZM functions,
and direct calls between Stratum, ASIC result handling, self-test, monitoring,
and presentation. This makes a board or driver addition change unrelated
features and makes partial implementations appear complete because the happy
path happens to be wired for one backend.

## Why it matters

Ownership leakage increases safety risk, prevents host testing, hides lifecycle
failures, and makes each new board/ASIC/protocol multiply the review surface.

## Stakeholders

- **Feature maintainer** — owns one policy/state machine with testable inputs.
- **Board/driver maintainer** — implements a stable capability/port contract.
- **Platform maintainer** — adapts ESP-IDF tasks, queues, transports, time, NVS,
  logging, and restart without exporting those mechanisms into domain policy.
- **Operator** — receives consistent behavior across supported hardware.

## Desired outcome

Every feature names its policy owner, portable data contract, required ports,
concrete adapters, composition point, and non-goals. Cross-feature exchange uses
owned commands, events, or immutable snapshots rather than shared mutable state.

## Primary flow

1. The composition root selects a board profile and concrete adapters.
2. It constructs feature contexts from portable contracts and explicit ports.
3. Feature services run policy without board IDs, driver headers, `GlobalState`,
   or ESP-IDF resource types.
4. Adapters translate domain commands/events to hardware, protocol, storage, or
   platform operations and return typed status.

## Alternate and failure flows

- Missing required capability or port prevents feature startup and publishes a
  precise unsupported/degraded result; it never falls through to a partial path.
- Optional capability absence follows an explicit alternative or skip policy.

## Non-goals

- This contract does not require a single large rewrite or one component per
  function; migration proceeds through independently testable seams.
- It does not ban ESP-IDF, board IDs, or concrete driver APIs inside their own
  adapters and composition code.
- It does not make every backend support every optional feature.
