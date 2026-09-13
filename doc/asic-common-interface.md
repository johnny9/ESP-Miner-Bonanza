# ASIC common-job interface

Bonanza now uses the interface proposed in
[`upstream-refactor/03-common-jobs-pr1969`](https://github.com/johnny9/skot-ESP-Miner/tree/0697551e957760033c94d1bccdb58c9a92e3c36d),
with the small compatibility extensions below. The preceding PR #1969 merge
at `68cb1c43` supplied the characterization tests; this change migrates the
production job representation and its consumers.

## Contract adopted from the proposal

- `components/mining_job` defines `asic_job_t` independently of ESP-IDF,
  Stratum parsers, and ASIC families. Hash arrays contain exact Bitcoin header
  bytes; integers use host byte order. `asic_job_header()` explicitly writes
  little-endian integers and retains the proposal's signature.
- Job ID and hexadecimal extranonce are inline arrays of 32 and 65 bytes,
  including terminators. A job, retained assignment, or snapshot can be copied
  by value. No metadata allocations, clone function, or destructor are needed.
- `mining_build_asic_job()` builds complete owned work from `miner_job_t`.
  Rejection leaves the destination unchanged. Version zero selects the source
  version, matching the proposal. Coinbase allocation failures still propagate
  as failures instead of producing work with a zero hash.
- `ASIC_send_job()` borrows the job for the duration of the call. The driver
  retains an independent value. Producers release their stack storage after
  the call, and result snapshots survive replacement and invalidation.
- `bm_job_build_from_asic_job()` converts common header bytes to Bitmain packet
  order. Its packet type and builder headers are private to the ASIC component.
  BZM builds its own midstates and register values from the same common job.

The old `mining_template_t`, nested heap metadata, and common
`ASIC_send_work()` entry point have been removed. The separate SV1/SV2 legacy
fixture builders also return `asic_job_t`.

## Minimum changes needed in the proposal

| Change | Reason | Scope |
| --- | --- | --- |
| `bool ASIC_send_job(...)`, rather than `void` | Busy, flush, initialization, or allocation failures must reach the producer. A rejected send retains its extranonce and pending clean boundary. | Generic acceptance contract; no driver-specific error enum. |
| `uint64_t work_generation` in the job | Results and queued input must retain the pool/session retirement identity across reconnects and clean updates. | Opaque identity; no engine or bridge fields. |
| `bool clean_jobs` in the job | The accepting driver must retire old assignments before admitting work at a clean boundary. A failed attempt cannot consume that boundary. | Existing pool-job semantics, also useful for local work replacement. |
| `uint32_t job_version` in the job | Share validation needs the original pool version even after the producer rolls `version` in software. | Protocol provenance, independent of ASIC family. |

These are the only additions to the proposed job type. Numeric SV2 job IDs and
binary extranonces are derived from its existing strings when constructing a
submission; they are not duplicated in common work. The parser uses an alias
of the proposal's source enum rather than maintaining a second protocol enum.

The private Bitmain adapter keeps Bonanza's allocation-free packet output and
existing one/four-midstate behavior. It consequently takes a source and an
output pointer, rather than the proposal's additional midstate-count argument.
That difference does not enlarge the common interface or change driver packets.

The producer uses `version_rolling` and `max_version_variants` from the existing
capabilities when refreshing a fixed-merkle job. BZM and BM1397 require a new
base version after each accepted finite-midstate job; Bonanza's legacy device
configuration flag must not suppress that progression. Version changes stay
within the negotiated mask, and rejected sends retain their version. This
requires no additional job fields or capability fields.

## Existing interfaces BZM must retain

The `03-common-jobs` proposal deliberately retains upstream's older result and
storage path. Importing those parts over Bonanza would regress existing support:

- Common results must identify work opaquely and carry the actual mined
  `final_ntime` and `final_version`. Bonanza already has `asic_event_t` for this.
  Engine IDs, wire sequence decoding, and timestamp-offset reconstruction stay
  in the driver. Header validation uses a copy of the saved job with its actual
  mined `ntime`; no new header-encoder argument is necessary.
- BZM needs all 236 logical engine assignments retained. Bonanza's 256-entry
  store and generation-bearing handles remain implementation details, not job
  fields or a mandatory common-interface capacity. Bitmain's hardware-slot
  compatibility mode remains available.
- Work clearing must preserve the existing driver flush/drain barrier and
  stale-result rejection. Logical invalidation is not physical power-off.
- Existing driver dispatch, capabilities, health, and board lifecycle operations
  remain in place. This migration does not import the broader refactor stack.

## Separate planned work

This representation migration preserves the current BZM timestamp scheduling
policy, including its configured rolling window and actual-time result checks.
It does not invent a new pool permission to roll time. The broader proposal
still needs an explicit, hardware-neutral per-job time allowance, with overflow,
exhaustion/refresh, and protocol-derived bounds. Hardware support and permission
for a particular job are separate concerns. Keep the existing fixed-time
Bitmain behavior when introducing that contract.

Per-job local diagnostic destinations and startup handoff also remain separate
work; current self-test routing still uses the existing self-test state. Neither
bridge lifecycle fields nor a BZM-specific diagnostic enum belong in this job.

## Validation

All existing packet, midstate, nonce-difficulty, clean-generation, retry,
retained-work, and result-routing regressions are retained. Common hash fixtures
now express header bytes; captured hardware packet/midstate vectors are unchanged.
The allocation-failure regression now injects failure into large-coinbase hashing,
since ordinary job metadata no longer allocates.

Additional shared tests cover the proposal's exact 80-byte encoder, value-copy
ownership and original-version/session metadata, unchanged output on rejected
builds, zero-version fallback, and invalid inline metadata. Native and QEMU
builds use the same test bodies. See [the test guide](unit_testing.md) for commands.

Validation on 2026-09-13:

- 385 tests under each of GCC and Clang with address, undefined-behavior, and
  leak checks; 479 QEMU tests pass. The existing five hardware/device-only
  exclusions remain outside QEMU.
- All 17 inventory/coverage tooling tests, 99 Axe-OS tests, and the Bitaxe 1002
  factory-configuration check pass.
- ESP-IDF 6.0.2 builds `esp-miner.bin` with 31% application-partition space free.
- Unchanged coverage gates pass: 77.1% line and 64.5% branch coverage within
  instrumented files; 50/128 first-party source files are instrumented. SV1
  protocol remains at 100% line/function and 92.4% branch coverage.

Logs are retained locally in `.cache/common-jobs-validation/`; coverage reports
are under `build/host-coverage/coverage/`. This validation did not flash a
physical miner or run the migrated image on `bonanza.local`; hardware mining
validation remains a separate gate.
