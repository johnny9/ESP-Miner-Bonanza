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
Wi-Fi and HTTP remain available, but local work does not wait for a network
connection or NTP. The existing factory flag and BOOT trigger are supported.

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

Readiness and worker exit have 15-second limits; warmup has a 120-second limit.
Invalid thermal settings are rejected before local work starts. Temperature
limits apply during warmup and the 30-second measurement. Unreliable domain
samples fail instead of falling back to an aggregate pass. A failed worker or
shutdown overrides success. Failed cleanup keeps the fan at full speed and the
factory flag set; cancellation can retry cleanup with BOOT or HTTP. The
successful ten-second restart delay is retained.

Bonanza still uses its existing measurement and thermal adapters. The typed
measurement store, general runtime controller, and shared fan policy from the
broader refactor remain separate prerequisites for a wholesale stage 15/16
migration.

## Network control

The existing local-network HTTP authorization and CORS policy applies.

- `GET /api/system/selftest` returns `status`, `active`, `cleanupConfirmed`,
  `workerRunning`, a progress message, and accepted/rejected local nonce counts.
- `POST /api/system/selftest` with `{"action":"start"}` requests a one-shot
  manual diagnostic boot after the existing verified restart guard. It returns
  202. An active/pending test or unavailable safe shutdown returns 409.
- `POST /api/system/selftest` with `{"action":"cancel"}` requests cancellation.
  The worker exits and power cleanup must succeed before restart. The factory
  flag is cleared only after confirmed cancellation cleanup; cancellation never
  records a pass.

Status is for the current boot. Poll it during a test to retain completion
before the automatic restart. A normal subsequent boot reports `idle`.
Malformed or oversized requests return 400 without starting diagnostics.

## Validation

Shared native/QEMU tests exercise the complete production worker and void
adapter with controlled scheduling and driver I/O. They cover BZM and Bitmain
version progression, identical retries, task creation failure, cancelled and
retired epochs, generation exhaustion, nonzero bounded waits, local/pool result
isolation, the characterized first header, and domain/deadline policy.

Local validation: 406 tests under GCC and Clang sanitizers, 503 QEMU tests, the
ESP-IDF firmware build, and unchanged coverage gates pass. Hardware validation
is recorded separately after the candidate is installed.

`tools/hil/test_self_test_regression.py` is a testcode extension. Point an ignored
`miner-test` profile's `runner.tests_dir` at this checkout's `tools/hil`, select
`test_self_test_regression.py`, and use the existing explicit writable network
profile and immutable application artifact. The extension uses testcode's
identity, OTA, baseline, cleanup, and artifact lifecycle. It covers completed
local mining/verified shutdown, duplicate and malformed starts, cancellation,
and normal reboot; it requires no USB. A separate normal-pool check verifies
restored settings and fresh public-pool shares after the suite.
