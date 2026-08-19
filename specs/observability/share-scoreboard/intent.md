# Best-share scoreboard — intent

## Problem

The miner records its best shares across restarts while mining and HTTP tasks
can access the list concurrently. A torn snapshot, unchecked indexed NVS key,
or schema mismatch makes the feature unreliable or unsafe.

## Why it matters

Operators need a stable top-share history without corrupting runtime state,
blocking forever, or receiving a response that disagrees with OpenAPI.

## Stakeholders

- **Mining path** — inserts a qualifying share without corrupting the list.
- **HTTP/AxeOS** — reads a consistent bounded snapshot with a finite timeout.
- **NVS layer** — persists only valid indexed entries.

## Desired outcome

The scoreboard owns a mutex-protected, sorted, bounded list. Persistence and API
serialization consume snapshots safely, and schema fields match the wire.

## Primary flow

1. A share qualifies for the bounded top list.
2. The scoreboard mutates and persists affected entries while holding its lock.
3. HTTP serializes a locked snapshot; AxeOS derives presentation-only rank/age.

## Alternate and failure flows

- Lock, allocation, parse, or serialization failure returns an observable
  failure and leaves the previous valid scoreboard intact.

## Non-goals

- Rank display order and relative-time text are not persisted firmware facts.
