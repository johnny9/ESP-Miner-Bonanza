# On-device display lifecycle

The display selects its hardware layout once, updates at bounded cadences, and
sleeps only when normal carousel state permits it.

- **Lifecycle:** supported
- **Owner:** firmware display and device-configuration maintainers
- **Last reconciled:** 2026-08-11
- **Spec ID:** UI-DISPLAY

[Intent](intent.md) · [Acceptance](acceptance.md) · [Design](design.md) ·
[Risks](risks.md)

## Changelog

- 2026-08-11: Defined display descriptor, backend-adapter, and typed status-snapshot boundaries.
- 2026-08-10: Captured merged display constraints from PRs #525 and #937 and
  reconciled current LVGL/activity/timer behavior.
