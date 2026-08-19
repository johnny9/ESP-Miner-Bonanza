# Bitmain ASIC backend — intent

## Problem

ESP-Miner supports several Bitmain ASIC generations with different command,
job, rolling, nonce, PLL, and result layouts. If higher mining layers switch on
chip model, or model drivers retain protocol-owned pointers, every new ASIC or
Stratum mode multiplies duplicated logic and stale-result risk.

## Why it matters

Correct packet construction and result attribution decide whether work hashes,
shares are submitted to the right pool job, frequency transitions stay within
device bounds, and new hardware can be added without editing Stratum code.

## Stakeholders

- **Miner/operator** — receives stable hashing and correctly attributed shares.
- **Stratum/mining tasks** — use one chip-independent template/result contract.
- **ASIC component maintainer** — owns Bitmain packet and result semantics.
- **Board/power maintainer** — owns supported model, count, frequency, voltage,
  reset, and serial initialization constraints.

## Desired outcome

One board selects one Bitmain backend behind the generic ASIC driver contract.
The backend consumes neutral ASIC work, privately encodes and tracks hardware
slots, and emits normalized raw results. Neutral provenance, candidate
validation, protocol routing, and feature policy remain outside the backend.

## Primary flow

1. Device configuration selects a Bitmain model and its capabilities, bounds,
   count, and driver operations.
2. Initialization resets/detects the chain, validates chip identity/count,
   configures registers/serial/frequency, and reports success or fails closed.
3. Neutral ASIC work is encoded as the selected Bitmain packet and associated
   with a private bounded hardware slot before send.
4. A raw UART result is decoded, tied to its neutral work handle, normalized,
   and returned to the mining service. The backend does not select a protocol
   submit path.

## Alternate and failure flows

- Unknown model, absent required operation, job-store failure, invalid chip ID,
  malformed serial result, unknown/stale slot, or failed send rejects work and
  leaves mining status observably unavailable/degraded.
- Register responses remain distinct events and never enter share submission.

## Non-goals

- This spec does not describe the Bonanza/BZM bridge protocol or require a
  mixed-ASIC chain.
- Pool/session lifecycle belongs to
  [Stratum client lifecycle and pool failover](../../mining/stratum-client-lifecycle/SPEC.md).
- Voltage policy and thermal shutdown belong to power/device-safety features.
- Neutral template/work ownership, provenance, candidate validation, result
  observers, and share routing belong to
  [Mining work and result pipeline](../../mining/work-pipeline/SPEC.md).
- Self-test applicability and pass/fail policy do not belong to the Bitmain
  backend.
