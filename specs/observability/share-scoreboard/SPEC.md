# Best-share scoreboard

Firmware keeps a bounded, persistent top-share list and exposes a schema-aligned
snapshot to AxeOS.

- **Lifecycle:** implementing
- **Owner:** firmware scoreboard and AxeOS observability maintainers
- **Last reconciled:** 2026-08-11
- **Spec ID:** OBS-SCOREBOARD

[Intent](intent.md) · [Acceptance](acceptance.md) · [Design](design.md) ·
[Risks](risks.md)

## Changelog

- 2026-08-11: Made scoreboard an observer of neutral mining candidate events rather than ASIC task logic.
- 2026-08-10: Reconciled merged PR #1236 constraints with current concurrency,
  NVS, HTTP, OpenAPI, and AxeOS behavior.
