# Power-management integration into master — 2026-09-18

Merged the three commits on `refactor/bzm-upstream-power` through
`0604cfde3f4edc4502334565e577d68f5d1c5f79` into Bonanza master at
`053730109e8a4872b60259e767c6de3c1c0514a9`. Both histories are preserved.

## Integration decisions

- Keep master's PR #1972 packet encoders, retained common-job/result snapshots,
  result-handler ownership, expanded tests, mDNS pin, and frontend changes.
  Register the power-policy and share-queue sources alongside those components.
- Use the common power owner and board operations. Master's regulator minimum
  voltage clamp is preserved through the effective configuration getter and
  the common policy's board limits; no legacy power loop remains.
- Keep generation retirement in the result-task characterization while moving
  send timing to the submission worker. Extend the snapshot regression to prove
  that the newer result's embedded job and context survive producer-buffer
  reuse and queue-entry relocation for all three pool protocols.
- Preserve the tested power/submission implementations without further
  production changes. Merge resolutions join the build/test registrations,
  adapt the result-task fixture, and reconcile equivalent voltage-floor code.

## Validation of the merged sources

- 436 native tests pass under GCC and Clang with address, undefined-behavior,
  and leak sanitizers.
- All 542 ESP32-S3 QEMU tests pass. The inventory contains 436 host-eligible
  and 111 host-excluded tests; five device/hardware tests remain outside QEMU.
- Inventory/tooling checks and coverage gates pass: 78.3% lines and 65.9%
  branches within instrumented files; 58 of 138 eligible production files are
  instrumented. These are not firmware-wide coverage percentages.
- ESP-IDF 6.0.2 builds the application with 31% partition space free, using the
  unchanged master frontend bundle. No frontend source was changed by this merge.
- Whitespace/conflict checks pass. Logs and build provenance are local under
  `.cache/master-power-validation/`; the firmware is in `build/firmware/`.

The earlier [hardware report](share-submission.md) applies to candidate
`bzm-pm-424e8c3ad032` before this master integration. This merge was not flashed
or separately tested on hardware; its new validation is native, QEMU, and
firmware compilation. The physical qualification limits in that report remain.
