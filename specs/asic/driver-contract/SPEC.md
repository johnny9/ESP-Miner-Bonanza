# ASIC driver contract

All ASIC families implement one opaque, capability-driven lifecycle, work,
diagnostic, telemetry, and health contract without exposing backend internals.

- **Lifecycle:** implementing
- **Owner:** firmware ASIC architecture and backend maintainers
- **Last reconciled:** 2026-08-11
- **Spec ID:** ASIC-DRIVER

[Intent](intent.md) · [Acceptance](acceptance.md) · [Design](design.md) ·
[Risks](risks.md)

## Changelog

- 2026-08-11: Defined a common Bitmain/BZM contract and explicit self-test,
  Stratum, board, application-state, and ESP-IDF boundaries.
