# Hostname, mDNS, and Origin safety

Configured identity, runtime mDNS conflict identity, and browser-origin
authorization remain distinct and bounded.

- **Lifecycle:** implementing
- **Owner:** firmware networking and HTTP security maintainers
- **Last reconciled:** 2026-08-11
- **Spec ID:** NET-IDENTITY

[Intent](intent.md) · [Acceptance](acceptance.md) · [Design](design.md) ·
[Risks](risks.md)

## Changelog

- 2026-08-11: Replaced the target `GlobalState` component interface with an identity status port.
- 2026-08-10: Reconciled PR #1240 review constraints with current hostname,
  mDNS task, system-info, Origin, NVS, and swarm behavior.
