# Protocol-neutral mining work pipeline — intent

## Problem

Neutral types exist, but they are split between the Stratum and ASIC components,
the job store retains protocol-specific submission fields, work generation casts
V1/V2 payloads, and the ASIC result task directly calls Stratum, self-test,
monitoring, and scoreboard implementations. Self-test also creates work by
parsing a hard-coded Stratum notification.

## Why it matters

Mining correctness requires one owner for work provenance and result identity.
Protocols should not know ASIC models, ASICs should not know pool routing, and
diagnostics should not impersonate a network protocol.

## Stakeholders

- **Stratum adapters** — publish validated upstream work and accept candidates.
- **ASIC facade/backends** — consume neutral work and publish normalized events.
- **Self-test** — requests diagnostic work and observes diagnostic results.
- **Observers** — consume immutable accounting/monitoring/scoreboard events.

## Desired outcome

A portable mining service owns typed work envelopes, source generations,
hardware handles, job provenance, result validation, share candidates, and
observer fan-out. Stratum, ASIC, and self-test integrate only through its ports.

## Primary flow

1. A work source publishes validated `mining_work_t` with source generation,
   purpose, and opaque upstream origin.
2. The pipeline derives/refreshes `asic_work_t`, stores provenance, and submits
   it through the ASIC port.
3. An ASIC event is normalized and matched to the exact stored work generation.
4. The pipeline validates a share candidate and delivers it to the originating
   share sink plus immutable observers.

## Alternate and failure flows

- Diagnostic work uses purpose `SELF_TEST`, a deterministic work factory, and a
  diagnostic observer; it does not require a pool session or share sink.
- Stale/unknown work, failed clone/queue/send, unsupported capability, or closed
  source generation rejects the operation without guessing current protocol.

## Non-goals

- This feature does not parse V1/V2 messages, encode BM/BZM packets, choose pool
  failover, evaluate self-test pass/fail, or render statistics.
- Protocol routing data remains opaque to ASIC backends; hardware slot data
  remains opaque to Stratum and self-test.
