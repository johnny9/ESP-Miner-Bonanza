# On-device display lifecycle — risks and scope

## Scope and review unit

- **In:** Layout selection, screen/carousel state, activity, sleep sentinels,
  timer cadence, local/external backend boundary, and evidence.
- **Out:** Detailed visual styling and external display firmware internals.
- **Smallest coherent change:** Sleep/activity changes include the state decision,
  settings contract, UI control, and focused tests.
- **Split out:** New hardware drivers and external-display releases.

## Assumptions and open questions

- Define which important events besides button, identify, and block-found wake or
  extend activity, and for how long.
- AC-06 needs test evidence at the current revision.

## Failure modes

| Failure | Impact | Detection | Mitigation |
|---|---|---|---|
| Setup screen sleeps | Recovery becomes difficult | State-table test/HIL | Pre-carousel exemption |
| Activity not reset | Button appears ineffective | Input test/HIL | LVGL activity trigger |
| Layout selected repeatedly | CPU/heap churn | Timer/profile test | Startup-only selection |
| Timeout overflow | Wrong immediate/never sleep | Max-bound test | Safe signed arithmetic |
| External backend receives local settings | Confusing/no-op control | Backend fixture | Capability-aware UI |

## Security, resources, and rollout

- Display text must not expose secrets from Wi-Fi or pool configuration.
- Bound LVGL allocations, timer frequency, string length, and update work.
- Preserve usable setup/recovery screens when rolling back display changes.
