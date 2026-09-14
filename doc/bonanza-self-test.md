# Local self-test and Bonanza

This adapts the local-work refactor and completion rules from the fork's
`upstream-refactor/15-common-self-test` and
`upstream-refactor/16-self-test-completion-fixes` (`69ae6990`) to Bonanza.
The stage 03 common-job header, encoder, builder signature, and void submission
signature remain unchanged. This does not import the rest of the upstream
runtime, measurement-store, or fan-policy stack.

## Work and results

Self-test builds an owned local Bitcoin job whose first header matches the
characterized legacy self-test. A dedicated worker submits it through the same
void ASIC adapter as pool work. It increments time and advances finite-midstate
versions only after acceptance. Delays always yield and are broken into at most
100 ms waits. Driver backpressure retains the borrowed job until acceptance or
cancellation.

Local destination and generation live in Bonanza's assignment context, outside
`asic_job_t`. Retained results keep this context with the job. Cancellation
revokes the local generation, including pending retries; stale local results
cannot reach a subsequent test or the pool. Pool results cannot become test
proof merely because diagnostics are active. BZM's validated local results feed
its existing difficulty-weighted per-chip counters and startup proof.

The self-test no longer parses fake Stratum messages, allocates a coinbase,
changes pool difficulty, creates a pool producer, or starts a protocol task.
Hardware self-test validation uses USB serial. Diagnostic boots leave Wi-Fi
and HTTP stopped, and local work does not wait for a network connection or NTP.
The existing factory flag and physical BOOT trigger are supported.

## BZM operation

Bonanza uses its production controller for startup, interlocks, and verified
OFF_SAFE cleanup. Diagnostics stay at the existing 800 MHz / 2.8 V baseline;
live tuning is not started and saved mining settings are not changed. Overheat
fails the diagnostic instead of starting automatic retuning/recovery. The
existing 36% board fan minimum still applies during diagnostic PID control.

Expected hashrate comes from the existing power/controller estimate, including
BZM's clock-to-hash relationship. Every chip/domain requires at least three
plausible, distinct, fresh samples and the existing hashrate bounds. At least
two accepted nonces and the aggregate target are required. BZM's domain readings
are nonce-derived, not a claim of independent hardware register evidence.
The board's maximum power is used when it has no calibrated self-test power
target; finite positive power and the existing voltage/fan checks are required.

Readiness and worker exit have 15-second limits. Warmup has a 120-second limit
for Bitmain and a 300-second limit for Bonanza's slower 800 MHz baseline.
The 55°C warmup target and all temperature/fan limits are unchanged.
Invalid thermal settings are rejected before local work starts. Temperature
limits apply during warmup and the 30-second measurement. Unreliable domain
samples fail instead of falling back to an aggregate pass. A failed worker or
shutdown overrides success. Failed cleanup keeps the fan at full speed and the
factory flag set; cancellation can retry cleanup with BOOT. The
successful ten-second restart delay is retained.

Bonanza still uses its existing measurement and thermal adapters. The typed
measurement store, general runtime controller, and shared fan policy from the
broader refactor remain separate prerequisites for a wholesale stage 15/16
migration.

## USB hardware validation

Connect the miner's ESP32 USB serial console and record a full diagnostic boot
with `idf.py -p <USB_PORT> monitor`. Use the existing factory self-test
configuration or physical BOOT trigger to enter diagnostics. Hold BOOT to
cancel a running test or retry cleanup after a failure.

Retain the serial log from initialization through completion and restart. It
must show local nonce counts, per-chip/domain results, confirmed cleanup with
the worker stopped, and `SELF-TEST PASS!`. A cancelled run must report
`SELF-TEST CANCELLED` after confirmed cleanup and restart without recording a
pass. Capture the subsequent normal mining boot as restoration evidence.

There are no HTTP self-test endpoints, network trigger flag, or network-only
self-test runner. Normal mining boots still provide the ordinary network API.

## Validation

Shared native/QEMU tests exercise the complete production worker and void
adapter with controlled scheduling and driver I/O. They cover BZM and Bitmain
version progression, identical retries, task creation failure, cancelled and
retired epochs, generation exhaustion, nonzero bounded waits, local/pool result
isolation, the characterized first header, and domain/deadline policy.

USB-only revision validation on 2026-09-14: 406 tests under each of GCC and Clang
sanitizers, 503 QEMU tests, and 99 Axe-OS tests pass. API client generation removes
the self-test controls.
ESP-IDF 6.0.2 builds the application with 31% partition space free. Unchanged
coverage gates pass: 78.5% line and 65.6% branch coverage within instrumented
files, with 55/133 first-party production files instrumented. These are not
firmware-wide coverage percentages.

Logs are retained in `.cache/self-test-usb-validation/`. This revision has not
been flashed or tested over USB: the available serial device has not been
identified as Bonanza. USB hardware validation remains pending.

### Historical network hardware validation

The following evidence is for the earlier HTTP-enabled candidate. Its HTTP
control and testcode self-test extension have since been removed. These runs
do not constitute USB hardware validation of the current revision.

Clean firmware commit `713420fc67956cfe0c1208e1a68afb88762712ea` was built as
`bzm-selftest-713420fc` and installed through HTTP OTA on Bonanza 1002. The
2,895,616-byte application SHA256 is
`5c6a503a15a595300530208f5b9436bd1d811fade765f02c1fadd96585e18ad1`.
Python testcode was pinned to `ec36db41270aad3e665237bf003046d8bc17e50e`;
USB/serial was disabled. Local protocol listeners used LAN port 4333.

| Testcode suite | Result | Run ID |
| --- | --- | --- |
| Local self-test and cancellation | 2 passed | `20260914T100722.197311Z` |
| SV1 regression | 12 passed | `20260914T101558.226253Z` |
| SV2 standard channel | 7 passed | `20260914T102027.647518Z` |
| SV2 extended channel | 7 passed | `20260914T102229.863166Z` |

The completed diagnostic recorded 429 accepted local nonces and three rejected
nonces, passed all four chip/domain checks, and reported confirmed shutdown with
the worker stopped. No pool work or accepted pool shares appeared during the
diagnostic boot. Warmup took approximately 248 seconds before the 30-second
measurement. Twenty-five paired HTTP observations of self-test and controller
temperatures differed by at most 0.21°C. Cancellation returned to normal mining;
malformed actions and duplicate starts were rejected.

Earlier candidate runs are retained as failure evidence. `1fd1bbb4` exposed
the temperature reader timestamping before waiting for the reactor snapshot,
then retaining its old cached reading when freshness validation failed. It also
exposed the test harness treating HTTP availability as completed ASIC startup.
`ba5de7ba` fixed both and confirmed that the inherited 120-second warmup deadline
still expired while this board was heating normally. The final build permits a
300-second Bonanza warmup with the same 55°C target and safety limits. Both
timeout runs confirmed shutdown; the final run passed with clean restoration.
Final domain validation also accepts a newer concurrent monitor sample while
requiring both the averaged and current observations to remain fresh.

Independent Python SHA256d audits verified 44 SV1 shares across 40 jobs and
eight SV2 standard shares across four jobs, with no invalid proofs or duplicate
headers. One SV1 raw-frame successor lacks a recorded job payload and cannot be
rehashed. The standard audit combines recorded submissions with the pinned
suite's fixed merkle/previous-hash fixture. Extended transcripts lack the
extranonce bytes required for independent rehashing; their coverage is the
protocol suite. Fallback suites and other physical ASIC models were not rerun
for this self-test change.

A separate final API check confirmed the original pools, worker identities,
operating settings, and board identity. It observed accepted public-pool shares
increase from four to six, four ASICs and 944 engines in MINING state at the
saved 1200 MHz / 3.0 V settings, and no fault or reboot. Ports 4333/4334 had no
remaining test listeners. A settled follow-up reported 1.50 TH/s, 12 accepted
public-pool shares and zero rejected shares. That run left the candidate
installed on `bonanza.local`.

Reports, immutable firmware provenance, failure runs, paired temperature
observations, offline audits, and reproduction profiles/scripts are retained in
`mining-qa-testcode-bonanza-publish/artifacts/self-test-713420fc/` (earlier runs
are under `self-test-1fd1bbb4/` and `self-test-ba5de7ba/`). Only Bonanza 1002 was
validated physically; the shared worker tests cover Bitmain version progression
without substituting for hardware qualification.
