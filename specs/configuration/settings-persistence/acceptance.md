# Settings validation and persistence — acceptance

## Functional behavior

- [x] **CFG-SETTINGS-AC-01:** All supplied settings are validated before the
  update loop; an invalid field produces HTTP 400 and the request queues no
  settings writes.
- [x] **CFG-SETTINGS-AC-02:** Boolean settings accept only their declared
  boolean/0-or-1 domain, rotation accepts only 0/90/180/270, display and
  protocol strings use explicit allowed values, and hardware-constrained
  tuning values consult device configuration.
- [x] **CFG-SETTINGS-AC-03:** Public `overheat_mode` input can request disable
  only; the safety controller remains authoritative while recovery is active.

## Persistence and compatibility

- [x] **CFG-SETTINGS-AC-04:** NVS key lookup and indexed access reject invalid
  enum values and indexes before dereferencing a descriptor or value.
- [x] **CFG-SETTINGS-AC-05:** Booleans are stored as U16 0/1, firmware floats
  are stored as strings compatible with factory configuration, and frequency
  keeps the deliberate legacy integer mirror.
- [x] **CFG-SETTINGS-AC-06:** Existing key/type transitions use explicit
  fallback readers or migrations rather than silent renames.
- [ ] **CFG-SETTINGS-AC-07:** OpenAPI, generated types, AxeOS controls, mocks,
  firmware descriptors, and field-specific validation agree for every public
  setting. The current OpenAPI rotation enum is `[0, 1]`, while firmware and UI
  use `0`, `90`, `180`, and `270`.

## Verification evidence

| Evidence class | Exact command, case, revision, target, result, and date |
|---|---|
| Source reconciliation | Inspected `check_settings_and_update`, `PATCH_update_settings`, and NVS descriptors/migrations at `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f` — AC-01 through AC-06 present; AC-07 contradiction found, 2026-08-10 |
| Focused regression | Not run; documentation-only reconciliation. Atomic rejection, type/range domains, and NVS migration cases still need focused tests. |
| AxeOS generation/test/build | Not run; documentation-only reconciliation. |
| Firmware build or device | Not run; no build, flash, or device operation was required or authorized. |

## Acceptance rule

Changes to a public setting are not complete until every affected layer agrees,
negative tests prove atomic rejection, and persistence compatibility is explicit.
