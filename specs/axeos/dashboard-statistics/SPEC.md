# Dashboard statistics selection

AxeOS selects chart series locally and requests only stable telemetry columns
from firmware.

- **Lifecycle:** supported
- **Owner:** AxeOS dashboard and firmware HTTP API maintainers
- **Last reconciled:** 2026-08-11
- **Spec ID:** AXE-CHARTS

[Intent](intent.md) · [Acceptance](acceptance.md) · [Design](design.md) ·
[Risks](risks.md)

## Changelog

- 2026-08-11: Recorded current telemetry, HTTP-adapter, and presentation module boundaries.
- 2026-08-10: Captured the merged chart-selection contract from PR #955 and
  reconciled it with the current API, service, mock, and dashboard.
