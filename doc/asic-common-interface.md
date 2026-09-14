# ASIC common-job interface

Bonanza now uses the interface proposed in
[`upstream-refactor/03-common-jobs-pr1969`](https://github.com/johnny9/skot-ESP-Miner/tree/0697551e957760033c94d1bccdb58c9a92e3c36d),
unchanged for the common-job contract. The preceding PR #1969 merge
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

## Internal bookkeeping around the unchanged contract

No change to stage 03's common-job interface is required for Bonanza.
`asic_job.h` and `asic_job.c` match the proposal byte for byte, and submission is
`void ASIC_send_job(GlobalState *state, const asic_job_t *job)`.
The earlier claim that three extra job fields and a boolean return were necessary
was incorrect: those requirements belong to Bonanza's implementation.

Bonanza retains `work_generation`, the original `job_version`, and `clean_jobs`
in its internal `asic_job_context_t`, alongside each saved assignment. A producer
binds this context to the exact borrowed job pointer for the submission call.
The store serializes those submission scopes and copies the job and context
under its existing lock. Unrelated or local jobs do not inherit that context.
Result handling snapshots both values atomically, then carries the existing
internal share record through Stratum submission. This preserves session
retirement and negotiated-version checks without adding fields to common work.

The void adapter retries driver rejection synchronously while the caller keeps
its borrowed job alive. Extranonce, version, and the pending clean boundary stay
fixed until acceptance. If the pool generation retires, the adapter returns and
the producer consumes the next notification. Invalidation does not acquire the
submission-scope mutex, so it can cancel a blocked submission. Driver-private
boolean acceptance remains an internal detail; no additional common queue or
submission parameter is needed.

Numeric SV2 job IDs and binary extranonces are derived from the proposal's
existing strings when constructing a submission. The parser uses an alias of
the proposal's source enum rather than maintaining a second protocol enum.

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

BZM derives four consecutive negotiated versions for each enhanced-mode job,
matching that producer stride. The earlier OR-based mask selection repeated
headers across neighboring engines as soon as the producer advanced the base.
The wire format, midstate byte order, and result microstate mapping remain the
same; only the four selected versions change. Zero-mask work retains four
identical FIFO entries for enhanced-mode sequence identity.

The private BZM duplicate cache identifies the actual mined header and its
pool/session/job/extranonce identity, rather than an engine assignment handle.
This rejects repeated shares when both extranonce and version rolling are
disabled. Its 256-entry bound is unchanged, and physical sequence reuse or PLL
transitions do not reset logical share identity. Work retirement still uses the
existing opaque handles and generation checks.

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
now express header bytes. Captured nonce proofs and base-version midstate vectors
are unchanged; BZM's selected-version and microstate expectations now cover the
consecutive version ranges required to avoid duplicate headers.
The allocation-failure regression now injects failure into large-coinbase hashing,
since ordinary job metadata no longer allocates.

Additional shared tests cover the proposal's exact 80-byte encoder, value-copy
ownership, separate original-version/session metadata, unchanged output on rejected
builds, zero-version fallback, and invalid inline metadata. Native and QEMU
builds use the same test bodies. See [the test guide](unit_testing.md) for commands.

Validation of exact stage 03 alignment on 2026-09-14:

- 397 native tests pass under both GCC and Clang with sanitizers, including
  retrying the identical borrowed job and cancelling work on session retirement.
- All 494 QEMU integration tests pass.
- Store tests verify atomic metadata copies, stale-handle rejection after slot
  reuse, and isolation from unrelated job pointers.
- All 17 inventory/coverage tooling tests pass. Existing coverage gates pass:
  78.1% line and 65.1% branch coverage within instrumented files, with 52/130
  first-party source files instrumented. These are not firmware-wide percentages.

Historical validation before exact stage 03 alignment, on 2026-09-13:

- 395 tests under each of GCC and Clang with address, undefined-behavior, and
  leak checks; 492 QEMU tests pass. The existing five hardware/device-only
  exclusions remain outside QEMU.
- All 17 inventory/coverage tooling tests, 99 Axe-OS tests, and the Bitaxe 1002
  factory-configuration check pass.
- ESP-IDF 6.0.2 builds `esp-miner.bin` with 31% application-partition space free.
- Unchanged coverage gates pass: 78.1% line and 65.1% branch coverage within
  instrumented files; 51/129 first-party source files are instrumented. SV1
  protocol remains at 100% line/function and 92.4% branch coverage.

Logs are retained locally in `.cache/common-jobs-validation/`; coverage reports
are under `build/host-coverage/coverage/`. The hardware follow-up build and native
logs are also retained with the testcode artifacts described below.

### Network hardware validation

Firmware commit `670f58f3` was built with ESP-IDF 6.0.2 as
`bzm-common-670f58f3` and installed on a physical Bitaxe Bonanza 1002 through
testcode's bounded HTTP OTA flow. Identity and the running version were verified;
USB/serial access was disabled. Testcode was pinned to `ec36db4`.

| Python testcode suite | Result | Run ID |
| --- | --- | --- |
| SV1 protocol regression | 12 passed | `20260913T182612.148626Z` |
| SV2 standard channel | 7 passed | `20260913T183101.289870Z` |
| SV2 extended channel | 7 passed | `20260913T183310.403656Z` |
| Focused SV1 fallback | 3 passed | `20260913T183536.559220Z` |

An additional offline Python SHA256d audit verified 42 SV1 submissions across
40 jobs and six SV2 standard submissions across four jobs, with no duplicates.
One SV1 raw-frame case omits its job payload from the transcript and cannot be
independently rehashed. The SV2 standard audit combines recorded submissions
with the pinned suite's fixed merkle-root/previous-hash fixture. The fake pools'
ACK policy alone does not independently verify proof of work.

The hardware runs exposed and now cover finite-midstate producer progression,
overlapping BZM versions, mask carry outside the negotiated bits, and duplicate
fixed-header shares across different engine assignments. These fixes add no
common job or capability fields. All protocol suites and the focused fallback
run restored the original pool entries and operating settings; a separate API check then observed fresh
accepted shares on the original public pool with four ASICs and 944 engines.

The earlier `781b5846` pool-fallback run `20260913T173814.368457Z` passed seven
cases, skipped the optional browser form case (no CDP session), and failed silent-primary
failover after 900 seconds. Cleanup restored the original pools. SV1's transport
read loop retried zero-byte polling timeouts indefinitely, bypassing the socket's
three-minute receive policy. The follow-up enforces a three-minute deadline for
a complete line, including partial frames, while preserving short pauses. Three
QEMU transport regressions cover silent reads, a short pause with fragmentation,
and partial-frame expiry with clean subsequent framing. Hardware revalidation
of normal failover, short silence and sustained silence uses the unchanged testcode methods selected through `unittest.load_tests`.
All three cases and their cleanup passed.
Silent-primary failover produced fresh accepted shares in 738.344 seconds
(900-second limit), and primary recovery passed in 85.668 seconds without a
reboot. The other five previously passing cases have not been repeated after
this transport-only fix.

Detailed local reports, immutable firmware provenance, failure analysis and hash
audits are retained in testcode's ignored `artifacts/common-interface-670f58f3/`
directory, with earlier failures retained in their revision-specific directories.
The initial OTA setup run `20260913T181928.624574Z` timed out awaiting its HTTP
response after the image had installed and rebooted. An independent API read
verified the new version, and the successful SV1 rerun recognized it without
another upload. That setup error remains in the artifact history.
