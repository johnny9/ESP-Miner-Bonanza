# Stratum session and message safety — intent

## Problem

Pools control message timing, identifiers, lengths, counts, certificates, and
session transitions. Unbounded or stale state can corrupt work, misattribute
latency, block failover, or exhaust an ESP32.

## Why it matters

Reliable mining requires protocol order, bounded parsing/copying, prompt share
submits, and full cleanup whenever a transport or Noise session ends.

## Stakeholders

- **Miner/operator** — receives valid work, accurate status, and timely failover.
- **Stratum transport/session** — owns sockets, Noise, buffers, and reconnect.
- **Mining/ASIC work path** — receives only validated owned templates.

## Desired outcome

V1 and V2 share common socket policy where applicable while keeping Noise,
transport, framing, and mining semantics separated. Every retained value has a
bound and owner, and reconnect resets all session-scoped state.

## Primary flow

1. Resolve and connect with bounded common socket options.
2. Complete protocol authorization/channel setup in order.
3. Parse bounded messages into protocol values, adapt valid jobs to neutral
   mining work, correlate responses by actual ID/sequence, and transfer
   ownership explicitly.
4. On timeout/error/reconnect, close transport and clear session-owned state.

## Alternate and failure flows

- Malformed, oversize, allocation-failed, certificate-invalid, or timed-out
  input aborts the affected operation/session safely and reports precise status.

## Non-goals

- This spec does not require one unified V1/V2 state machine; an unresolved
  historical suggestion alone does not establish that architecture.
- Pool selection, protocol-task lifecycle, work-queue epochs, and failover belong
  to [Stratum client lifecycle and pool failover](../stratum-client-lifecycle/SPEC.md).
- ASIC selection, work dispatch, result validation, and diagnostic mining belong
  to [Mining work and result pipeline](../work-pipeline/SPEC.md). Stratum does
  not inspect concrete ASIC or board configuration.
