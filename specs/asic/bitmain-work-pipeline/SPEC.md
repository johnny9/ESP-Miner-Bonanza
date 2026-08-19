# Bitmain ASIC backend

Bitmain models implement one generic ASIC driver boundary, privately encode
hardware work, and return normalized raw results to the mining service.

- **Lifecycle:** implementing
- **Owner:** firmware ASIC and mining maintainers
- **Last reconciled:** 2026-08-11
- **Spec ID:** ASIC-BITMAIN

[Intent](intent.md) · [Acceptance](acceptance.md) · [Design](design.md) ·
[Risks](risks.md)

## Changelog

- 2026-08-11: Narrowed Bitmain to a concrete ASIC backend; neutral work,
  provenance, validation, observers, and protocol routing belong to MINE-WORK.
- 2026-08-10: Created from current BM1397/BM1366/BM1368/BM1370 code and
  merged maintainer constraints in PRs #478, #745, #747, #986, #1051, #1168,
  #1321, #1557, #1608, and #1779.
