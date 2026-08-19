# Stratum session and message safety

Stratum V1/V2 connections use bounded messages, explicit ownership, correlated
responses, finite transport waits, and clean reconnect boundaries.

- **Lifecycle:** implementing
- **Owner:** firmware Stratum and mining-protocol maintainers
- **Last reconciled:** 2026-08-11
- **Spec ID:** MINE-STRATUM

[Intent](intent.md) · [Acceptance](acceptance.md) · [Design](design.md) ·
[Risks](risks.md)

## Changelog

- 2026-08-11: Removed ASIC/board authority from the target protocol boundary
  and assigned neutral work ownership to the mining pipeline.
- 2026-08-10: Consolidated merged maintainer constraints from PRs #717, #913,
  #1063, #1292, #1391, #1413, #1553, #1722, and #1783.
- 2026-08-10: Split pool/task/failover ownership into MINE-CLIENT and reconciled
  the current V1 close-without-destroy contradiction.
