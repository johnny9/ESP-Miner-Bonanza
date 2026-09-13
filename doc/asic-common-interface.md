# ASIC common-interface alignment

Upstream [PR #1969](https://github.com/bitaxeorg/ESP-Miner/pull/1969), at
`7caf30f909f29ab776bc3d270c61f6e919ecedf5`, characterizes the job and result
path before the common-interface refactor. Its integration into Bonanza keeps
that tests-only purpose: production driver, task, and protocol behavior is
unchanged. The tests now exercise Bonanza's existing common-work boundary.

This follows the workspace's September 13 `ASIC_REFACTOR_PLAN.md`: upstream
is the design authority; characterize behavior first, then migrate interfaces
in separate, buildable changes. The older 17-branch refactor series remains
reference work, rather than a prerequisite imported by this merge.

## Contract mapping

| Planned boundary | Current Bonanza implementation | Assertions retained or added |
| --- | --- | --- |
| Parsed pool input | `miner_job_t` | SV1, SV2 Standard and Extended fixtures, metadata limits, coinbase/extranonce bytes |
| Owned common work (`asic_job_t` in the plan) | `mining_template_t` and its owned `share` metadata | Detached job ID/extranonce, pool identity, clean generation, allocation failure and recovery |
| Driver submission (`send_job`) | `ASIC_send_work` / `asic_driver_t.ops.send_work` | Caller lends work; driver keeps its own snapshot; failed acceptance retains extranonce and the pending clean boundary |
| Family-private encoding | `bm_job_build` plus the five Bitmain senders | Original packet/CRC vectors and all eight captured BM1397 midstate vectors |
| Common results | `asic_event_t`, `asic_result_t` | Opaque work handle, final version, actual header time, separate receive timestamp, share/register routing |
| Owned result snapshot (`snapshot_job`) | `asic_job_store_snapshot` | Metadata survives slot replacement; generated handles expire on reuse; snapshots survive invalidation |
| Clean/session retirement | `work_generation`, `stratum_work_is_current`, and the store | Retired input cannot allocate/send; retired results cannot submit or receive later credit |
| Work invalidation (`clear_work`) | Driver clean barrier plus common-store invalidation | Existing Bonanza clean-job and safe-off regressions remain in QEMU |

The common job tests do not reintroduce `bm_job` into the pool or application
path. Private midstate assertions live in an ASIC test helper which runs the
real `bm_job_build`; common fixtures pass it only a `mining_template_t`.
The packet fixtures call the real sender with borrowed packet and common-work
values, then check an independent snapshot rather than pointer ownership.

## Deliberate differences from the upstream baseline

- Bonanza duplicates two metadata strings instead of allocating the old
  metadata-bearing `bm_job`. Allocation counts reflect those real calls.
- A failed build/send does not advance extranonce or decode coinbase for the
  UI. The next accepted cycle retries the same work.
- Work retired during a submission callback is not subsequently scored or
  credited as current mining proof. The callback still owns its metadata.
- The Bitmain builder produces one midstate for a zero mask and four for a
  nonzero mask. A common job has no software-midstate buffer/count.
- The SV1 submission vector uses PR #1940's portable escaped encoder and
  Bonanza's optional version-bit pointer. Transport integration remains in
  `test_sv1_transport.c`.

## Remaining production-interface work

This integration supplies regression coverage for the planned migration; it
does not claim that every planned interface is implemented already.

The planned `asic_job_t` stores header-byte-order hashes and self-contained
metadata. Bonanza currently keeps word-ordered hashes in `mining_template_t`
and allocates its metadata; its hashing and packet helpers perform the existing
conversions. Moving to the planned representation must preserve these vectors.

The result path already carries actual `final_ntime` through validation,
submission, and scoring. An explicit per-job rolling allowance, overflow and
exhaustion rules, and complete time-aware duplicate identity still need their
own production contract and tests. A driver capability alone does not grant
permission to roll a pool job's timestamp.

Local diagnostics also need an explicit work destination and startup-handoff
rules. Existing self-test routing uses the current self-test state; the new
characterization does not present it as the planned per-job destination.

Driver-owned snapshot dispatch, bounded receive/submission service behavior,
and any representation/API renames belong in the corresponding follow-ups.
Preserve Bonanza's 256-slot retention and its hardware clean/safe-off barriers.
Invalidating logical work is not proof of physical shutdown. Hardware mining
comparisons remain required for production dispatch or hardware changes.

## Fixture and validation boundaries

All 27 upstream scenarios are adapted, with five additional common-work cases.
Each test body is shared by native and QEMU builds. QEMU fixtures use the real
`GlobalState`; native driver/task instances use a shared explicit state view
and never exchange state objects with firmware instances. The complete real
sources are compiled; doubles replace scheduling, UART, pool submission,
accounting, and selected allocation calls.

The coverage runner scans test-instance object directories so their included
production drivers/tasks contribute coverage. Fixture source lines remain
excluded. Reports include instrumented inline-header code as well as the
complete first-party C/C++ source inventory; depth describes only instrumented
code, not the whole firmware.

Run the shared suites and coverage as described in [the test guide](unit_testing.md).
After adding a QEMU source file, reconfigure its ESP-IDF build so the component
source glob is refreshed. No physical miner is flashed by these checks.

Validation on 2026-09-13:

- 381 tests under each of GCC and Clang with ASan, UBSan and leak checks.
- 475 QEMU tests; the existing five `[not-on-qemu]` cases stay outside that run.
- 17 inventory/coverage tooling tests and 99 Axe-OS tests.
- ESP-IDF 6.0.2 firmware build and Bitaxe 1002 factory-configuration check.
- All unchanged coverage gates pass: 76.5% line and 63.9% branch depth;
  SV1 protocol remains at 100% line/function and 92.4% branch coverage.
  The producer has 100% line/89.5% branch coverage and the result task has
  92.8% line/81.2% branch coverage. All five Bitmain driver files now appear
  as instrumented in the report.

The pre-merge baseline at `21f1ccb4` had 349 native and 443 QEMU cases.
All existing test cases are retained. These checks do not include hardware
flashing or a new network mining run.
