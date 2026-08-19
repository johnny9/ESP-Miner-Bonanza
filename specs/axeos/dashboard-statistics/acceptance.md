# Dashboard statistics selection — acceptance

## Functional behavior

- [x] **AXE-CHARTS-AC-01:** Y1/Y2 choices are stored under browser local
  storage and are not represented by NVS settings.
- [x] **AXE-CHARTS-AC-02:** Query values use stable enum keys, not translated or
  human-readable chart labels.
- [x] **AXE-CHARTS-AC-03:** `/api/system/statistics` accepts a column list and
  returns `labels` plus rows whose values follow exactly that label order.
- [x] **AXE-CHARTS-AC-04:** The AxeOS service deduplicates mandatory and selected
  series and its mock returns the same ordered shape.
- [ ] **AXE-CHARTS-AC-05:** Focused tests cover persisted selections, duplicate
  Y1/Y2 fields, unknown columns, optional hardware series, and response-order
  mapping.

## Interfaces and resources

- [x] **AXE-CHARTS-AC-06:** OpenAPI documents the optional `columns` query and
  the labels/statistics response shape.
- [x] **AXE-CHARTS-AC-07:** History is bounded by firmware `statsLimit`, and UI
  updates are throttled rather than tied to every live-info event.

## Verification evidence

| Evidence class | Exact command, case, revision, target, result, and date |
|---|---|
| Source/schema reconciliation | Inspected statistics handler, OpenAPI, `SystemApiService.getStatistics`, local storage, and `HomeComponent` at `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f` — AC-01 through AC-04, AC-06, AC-07 present, 2026-08-10 |
| Focused frontend regression | Not run; documentation-only reconciliation. AC-05 remains unchecked. |
| API generation/build | Not run; no schema was changed in this documentation update. |
| Device/HIL | Not applicable to recording this browser/API contract. |

## Acceptance rule

Any new series must add one stable key and reconcile firmware, OpenAPI,
generated types, service/mock mapping, units, UI availability, and tests.
