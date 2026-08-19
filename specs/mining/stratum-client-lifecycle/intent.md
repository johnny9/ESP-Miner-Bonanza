# Stratum client lifecycle and pool failover — intent

## Problem

The Stratum client spans persisted pool configuration, a protocol coordinator,
V1/V2 tasks, a shared work queue, share submission, and mining-power state. If
those pieces disagree about the active pool, payload type, transport owner, or
task state, failover can leak resources, free work incorrectly, submit against
the wrong session, or report mining while no client task exists.

## Why it matters

An operator expects the miner to stay on a deliberately selected fallback,
recover automatically after an outage, conserve power when every pool is down,
and resume without stale work or state.

## Stakeholders

- **Miner/operator** — sees accurate active-pool status and predictable recovery.
- **Protocol coordinator** — owns pool and protocol lifecycle transitions.
- **V1/V2 session task** — owns one session's transport and protocol resources.
- **Mining work/result tasks** — consume work and submit shares only for a live,
  matching session.

## Desired outcome

Exactly one active client session has explicit ownership at a time. Pool changes
invalidate old work, every queued item carries immutable type/lifetime metadata,
and allocation, task-start, transport, or setup failure produces a real state
transition instead of a phantom running state.

## Primary flow

1. Load the configured `pool` and optional `fallback pool`, including each
   protocol, and start the selected protocol task.
2. After complete protocol setup, publish the active pool/protocol and emit
   typed, owned work through the mining work-source port.
3. On bounded retry exhaustion, try the other configured pool; if all pools are
   unavailable, pause mining/power and probe for recovery.
4. Resume on a reachable pool after old session state, queued work, and ASIC job
   metadata have been invalidated.

## Alternate and failure flows

- A user-selected fallback remains selected and does not automatically switch
  back to primary.
- A task/queue allocation or protocol-task start failure enters the same
  observable failure/recovery path as a connection failure.
- A shutdown timeout leaves the client failed/paused; it must not start a second
  session over resources still owned by the first.

## Non-goals

- Message parsing, framing, and cryptographic details belong to
  [Stratum session and message safety](../stratum-session-safety/SPEC.md).
- Neutral work dispatch, provenance, ASIC execution, result observation, and
  share routing belong to
  [Mining work and result pipeline](../work-pipeline/SPEC.md).
- Stratum policy does not query a concrete ASIC, board configuration, or global
  device aggregate. Mining capabilities are supplied as an immutable snapshot.
- More than the current two public pool slots is not required by this spec.
