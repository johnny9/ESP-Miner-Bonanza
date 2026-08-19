# Settings validation and persistence

Device settings are validated as one request and stored with explicit type and
upgrade/downgrade compatibility rules.

- **Lifecycle:** implementing
- **Owner:** firmware configuration and HTTP API maintainers
- **Last reconciled:** 2026-08-11
- **Spec ID:** CFG-SETTINGS

[Intent](intent.md) · [Acceptance](acceptance.md) · [Design](design.md) ·
[Risks](risks.md)

## Changelog

- 2026-08-11: Defined the immutable hardware-profile seam between settings policy and board configuration.
- 2026-08-10: Reconciled the settings contract with current code and review
  constraints from PRs #880, #1051, #1152, #1236, and #1331.
