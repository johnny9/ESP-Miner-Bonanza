# Swarm mixed-version compatibility — intent

## Problem

A browser served by one miner monitors peers that may run older or newer
firmware and omit fields from the local generated API. Treating every peer as
the local exact schema breaks mixed-version fleets.

## Why it matters

An operator must be able to retain, refresh, identify, and navigate to peers
without one missing field or unreachable device corrupting the entire swarm.

## Stakeholders

- **Operator** — sees available peers and clear per-peer failures.
- **System API service** — separates local generated calls from remote URI calls.
- **Swarm component** — owns tolerant normalization and browser-local state.

## Desired outcome

Local calls use the generated client. Remote calls use a bounded wrapper with
explicit URI, timeout, mocks, and compatibility fallbacks for missing fields and
identity.

## Primary flow

1. AxeOS discovers or loads a peer connection address.
2. The service fetches remote info/ASIC data with a finite timeout.
3. The swarm normalizes available fields, retains the connection address, and
   renders a compatible model/link.

## Alternate and failure flows

- One unreachable or older peer yields a per-device unavailable state or
  fallback values without failing the whole refresh.

## Non-goals

- Swarm browser state is not authoritative device configuration.
- Generated local API types are not proof that every remote peer has each field.
