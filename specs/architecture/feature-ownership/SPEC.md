# Feature ownership and dependency direction

ESP-Miner features depend on portable contracts and explicit ports; concrete
boards, ASIC backends, and ESP-IDF mechanisms remain behind adapters composed
at the application boundary.

- **Lifecycle:** implementing
- **Owner:** firmware architecture and feature maintainers
- **Last reconciled:** 2026-08-11
- **Spec ID:** ARCH-OWNERSHIP

[Intent](intent.md) · [Acceptance](acceptance.md) · [Design](design.md) ·
[Risks](risks.md)

## Changelog

- 2026-08-11: Defined the ownership, ports/adapters, composition-root, and
  incremental migration model after reconciling current self-test, Stratum,
  ASIC, thermal, power, board, and ESP-IDF dependency leaks.
