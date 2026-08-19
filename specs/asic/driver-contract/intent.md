# ASIC driver contract — intent

## Problem

The current driver table improves model dispatch, but driver operations still
take `GlobalState`, public ASIC headers include Stratum-owned mining types, and
the ASIC component reaches into private `main/` headers. Bitmain and BZM also
have different lifecycle/health depth, so generic consumers depend on whichever
backend happens to expose the needed detail.

## Why it matters

Stratum, self-test, power, monitoring, and API code need stable capabilities and
runtime outcomes without knowing BM registers, BZM bridge state, board IDs, or
ESP resource ownership.

## Stakeholders

- **ASIC backend maintainer** — implements model-specific mechanics privately.
- **Mining/self-test/safety features** — consume one explicit port contract.
- **Composition root** — supplies board profile and concrete dependencies.

## Desired outcome

An opaque ASIC instance exposes immutable descriptor/capabilities, purpose-aware
lifecycle, neutral work/results, validity-aware telemetry/health, and typed
failures. Bitmain and BZM are interchangeable at the feature boundary while
retaining backend-specific implementation and qualification.

## Primary flow

1. Composition selects a backend and creates an instance from a board profile
   plus required platform/hardware dependencies.
2. The caller prepares and starts it for mining, self-test, or maintenance.
3. Neutral work is submitted and normalized events/snapshots are consumed.
4. Stop is reason-aware, idempotent, and returns the hardware to its promised
   safe state before teardown.

## Alternate and failure flows

- Missing required operation/profile data rejects construction/startup.
- Unsupported optional capability is reported distinctly from runtime failure.
- Backend fault produces a stable fault snapshot and defined recovery action.

## Non-goals

- The contract does not expose BM/BZM registers, UART/bridge packets, job-slot
  formats, regulator details, or ESP task handles.
- It does not make the ASIC backend the owner of pool, self-test pass/fail,
  thermal policy, board qualification limits, or UI behavior.
