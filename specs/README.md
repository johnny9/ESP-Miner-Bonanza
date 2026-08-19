# ESP-Miner specifications

This directory holds durable, reviewable feature contracts for ESP-Miner. It
does not replace the README, the API schema, source code, or test results.
Specs explain the behavior and constraints that must survive implementation
changes; code, tests, builds, and hardware measurements provide evidence.

Start with [OVERVIEW.md](OVERVIEW.md), then find a feature in
[INDEX.md](INDEX.md). Use [_template/](./_template/) when creating a new
feature slice and follow [.agents/skills/specs/SKILL.md](../.agents/skills/specs/SKILL.md).

Apply [REVIEW-PREFERENCES.md](REVIEW-PREFERENCES.md) while planning and
reviewing a spec. It records evidence-derived expectations for sources of
truth, dependency direction, cross-layer API/configuration changes, ownership,
failure paths, patch scope, naming, and verification.
For module ownership and portability, apply
[feature ownership and dependency boundaries](architecture/feature-ownership/SPEC.md):
portable feature policy depends inward on value contracts and ports; concrete
board, driver, ESP, protocol, and storage implementations are adapters selected
by composition.

Feature documents use five companion files:

- `SPEC.md` — identity, lifecycle, ownership, and changelog.
- `intent.md` — problem, outcome, flows, and non-goals.
- `acceptance.md` — independently verifiable criteria and evidence.
- `design.md` — durable responsibilities, exact current module pointers,
  current boundary status, target interfaces, constraints, and recovery.
- `risks.md` — scope, assumptions, failure modes, and rollout/rollback.

When a feature constraint comes from historical review, record its provenance
in `design.md`: the PR, reviewer, merge and thread disposition, current-code
corroboration or contradiction, and whether an unresolved item is only a
candidate. The compact provenance belongs with the feature; raw review data
does not.

`SPEC.md` files outside `_template/` must be listed exactly once in
[INDEX.md](INDEX.md). Do not put unresolved template placeholders in a live
feature specification.
