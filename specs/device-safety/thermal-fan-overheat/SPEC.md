# Thermal, fan, and overheat control

Hardware-owned sensor facts feed a bounded fan controller and a fail-safe mining
power lifecycle in which overheat outranks normal pause state.

- **Lifecycle:** implementing
- **Owner:** firmware thermal, power, ASIC, and board-configuration maintainers
- **Last reconciled:** 2026-08-11
- **Spec ID:** SAFE-THERMAL

[Intent](intent.md) · [Acceptance](acceptance.md) · [Design](design.md) ·
[Risks](risks.md)

## Changelog

- 2026-08-11: Separated portable safety policy from Bitmain/BZM board, bridge,
  regulator, reset, and ESP adapters.
- 2026-08-10: Reconciled feature constraints from PRs #747, #800, #962,
  #1172, #1304, and #1608 with current thermal/fan/power code.
