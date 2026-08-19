# Stratum client lifecycle and pool failover

The mining client owns one explicit active pool/session, typed work handoff,
bounded failover, and complete shutdown/recovery transitions.

- **Lifecycle:** implementing
- **Owner:** firmware Stratum and mining-task maintainers
- **Last reconciled:** 2026-08-11
- **Spec ID:** MINE-CLIENT

[Intent](intent.md) · [Acceptance](acceptance.md) · [Design](design.md) ·
[Risks](risks.md)

## Changelog

- 2026-08-11: Restricted Stratum to pool/session policy plus work-source and
  share-sink ports; ASIC execution and results move to the mining service.
- 2026-08-10: Created from current client/coordinator code and merged review
  constraints in PRs #717, #913, #1422, #1553, and #1722.
