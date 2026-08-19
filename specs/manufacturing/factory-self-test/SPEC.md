# Factory self-test lifecycle

Self-test owns its synchronization, retained messages, temporary measurements,
safe power state, and terminal restart/reset behavior.

- **Lifecycle:** implementing
- **Owner:** firmware manufacturing/self-test and hardware-safety maintainers
- **Last reconciled:** 2026-08-11
- **Spec ID:** MFG-SELFTEST

[Intent](intent.md) · [Acceptance](acceptance.md) · [Design](design.md) ·
[Risks](risks.md)

## Changelog

- 2026-08-11: Separated portable self-test policy from board/ESP adapters and
  moved diagnostic hashing to the protocol-neutral mining pipeline.
- 2026-08-10: Captured explicit WantClue lifecycle constraints from merged PR
  #1615 and recorded retained stack-pointer gaps in current code.
