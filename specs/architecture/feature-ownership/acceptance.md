# Feature ownership and dependency direction — acceptance

## Architecture behavior

- [x] **ARCH-OWNERSHIP-AC-01:** Every live feature spec states non-goals,
  in/out scope, involved modules, authority, relationships, and current boundary
  status; structural validation enforces those sections.
- [ ] **ARCH-OWNERSHIP-AC-02:** Portable domain headers depend only on standard
  C and other portable domain contracts—not ESP-IDF, `main/`, `GlobalState`,
  board IDs, Stratum codecs, or concrete ASIC backends.
- [ ] **ARCH-OWNERSHIP-AC-03:** Feature policy services receive explicit context
  and ports; they do not include or call concrete driver/regulator/board APIs or
  use `GlobalState` as their interface.
- [ ] **ARCH-OWNERSHIP-AC-04:** Reusable components do not add private include
  paths into `main/` or depend upward on application feature modules.
- [ ] **ARCH-OWNERSHIP-AC-05:** Cross-feature work uses owned commands, immutable
  snapshots, or typed events with explicit lifetime, version, and failure rules.
- [ ] **ARCH-OWNERSHIP-AC-06:** One composition root selects concrete board,
  ASIC, thermal, power, storage, transport, and platform adapters; generic tasks
  do not repeat board/backend selection.
- [ ] **ARCH-OWNERSHIP-AC-07:** Architecture checks reject forbidden includes
  and CMake dependencies, while host tests can instantiate feature policy with
  fake ports and no ESP-IDF runtime.
- [ ] **ARCH-OWNERSHIP-AC-08:** Each migrated feature removes its compatibility
  path only after unit, integration/QEMU, and applicable authorized board/HIL
  evidence at the exact revision.

## Verification evidence

| Evidence class | Exact command, case, revision, target, result, and date |
|---|---|
| Specification audit | All live feature documents were reconciled for required boundary sections and module pointers at `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f`, 2026-08-11. |
| Source reconciliation | Current include/CMake relationships contradict AC-02 through AC-06; representative violations are recorded in design. |
| Architecture/unit checks | Not implemented or run; AC-07 remains unchecked. |
| Firmware/QEMU/HIL | Not run; this change defines contracts only. |

## Acceptance rule

A feature is not complete merely because one concrete board works. Its required
ports, unsupported behavior, ownership, cleanup, and portable policy tests must
be complete, and backend-specific claims require the appropriate target evidence.
