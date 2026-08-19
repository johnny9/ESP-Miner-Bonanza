# Specification maintenance checklist

Run this checklist for a spec-worthy change or before review.

- [ ] The feature is listed exactly once in `specs/INDEX.md` and its lifecycle
  matches `SPEC.md`.
- [ ] `SPEC.md`, `intent.md`, `acceptance.md`, `design.md`, and `risks.md` are
  present and linked.
- [ ] The changelog and reconciliation metadata describe current evidence.
- [ ] Acceptance criteria are independently testable; only current evidence is
  marked complete.
- [ ] Design pointers, API/schema details, hardware constraints, compatibility,
  resource limits, and recovery behavior are current where applicable.
- [ ] Every design names exact current modules, marks conforming and leaking
  boundaries, and identifies the target owner plus port/adapter/composition seam;
  vague placeholders such as “existing domain APIs” are not used.
- [ ] The authoritative owner of each changed hardware fact, state value, and
  identifier is named; consumers do not re-derive or shadow it.
- [ ] Feature policy does not use `GlobalState` or concrete board, ASIC, driver,
  ESP-IDF, transport, or persistence implementations as its interface. Any
  necessary integration is behind a narrow port and selected at composition.
- [ ] Applicable firmware, OpenAPI/protocol, generated client, AxeOS service,
  mock, UI, persistence, migration, and test surfaces are reconciled.
- [ ] Input bounds, resource ownership/lifetime, failure paths, task/callback
  behavior, and transport/session reset boundaries are explicit where relevant.
- [ ] The change is the smallest coherent review unit, or the reason a broader
  atomic change is required is recorded.
- [ ] The overview or story map changed if a project-level capability, actor,
  boundary, or user flow changed.
- [ ] No non-template specification contains an unresolved template placeholder
  or broken
  relative Markdown links.
- [ ] Applicable repository verification and `git diff --check` were run, or
  any limitation is reported precisely.
