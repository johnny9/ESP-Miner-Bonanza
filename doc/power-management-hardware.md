# Bonanza power-management hardware test — 2026-09-18

This is the initial candidate's historical report. The subsequent
[share-submission fix and validation](share-submission.md) addresses the
backpressure failure recorded here.

**Result: qualification blocked by result-queue overflow during sustained
TCP backpressure.** The original firmware was restored and fresh accepted
shares verified. No firmware fix is included in this test report.

## Provenance and access

- Device: Bonanza board 1002, four BZM ASICs, 944 active engines; original
  target 1200 MHz / 3000 mV.
- Candidate: `bzm-pm-745b3db0a7da`, installed in `ota_1` using HTTP OTA.
- Base commit: `8476539327e5aa723615efb2cc411adb8acfa1f2`, with uncommitted
  power-management/interface changes captured in the source archive.
- Source digest:
  `745b3db0a7daf9843f207e3a658b8961b0d00be4150ac828d909c0338c34b048`.
- Binary SHA-256:
  `d177fe590c31f46431a4ac9240d7df4017f8355d22bb2ecb5cff40de4074a620`
  (2,890,240 bytes; ESP-IDF 6.0.2).
- Original firmware: `bzm-selftest-713420fc`, preserved in `ota_0`.
- Testcode: `ec36db41270aad3e665237bf003046d8bc17e50e`, isolated clean
  checkout plus ignored local profiles and focused fixtures. The focused
  fixtures are local additions, not files from that published commit.

The requested SSH host was reachable but rejected the available key. Tests
used direct LAN HTTP, WebSocket logs, OTA, and local Stratum servers. There
was no serial/USB access or physical power switching. Settings and device
identity were compared against preflight hashes without publishing pool
credentials.

The firmware archive, patch, manifest, ELF, binary, and build log are in
`.cache/power-hardware/20260918T164341Z/`. Test evidence is in the sibling
`mining-qa-bzm-power-hardware/artifacts/power-management/` directory. These
artifacts are local and ignored; they are not part of this repository.
`local-test-sources.json` records hashes of the preserved local helpers and
focused fixtures.

## Passed checks

| Check | Evidence |
| --- | --- |
| OTA startup | Candidate version/partition verified; reached four ASICs, 944 engines, target settings, and valid mining |
| Stratum V1 | 12/12 cases passed; run `sv1/20260918T164944.433237Z` |
| Stratum V2 standard | 7/7 cases passed; run `sv2-standard/20260918T170844.566431Z` |
| Stratum V2 extended | 7/7 cases passed; run `sv2-extended/20260918T171128.414075Z` |
| Tuning down | Sampled 1200→1100 MHz in 25 MHz steps at 3000 mV, then 2950 mV |
| Tuning up | Sampled 2950→3000 mV at 1100 MHz, then 25 MHz steps to 1200 MHz |
| Pause/resume | Two cycles; ten-second paused holds had zero reported core voltage/current/frequency and accessible bridge info; resumed result processing |
| Pause during tuning | Interrupted an active down-ramp at an intermediate clock; safe shutdown and successful resume |
| Pool fallback | Manual selection, automatic failover/recovery, both pools unavailable then primary recovery, and both unavailable then fallback recovery |
| Pool-loss shutdown | Both 90-second outages produced SAFE_OFF with zero reported core voltage and unchanged user pause intent; recovery produced fresh shares |
| Short pool silence | Result processing and telemetry continued during 15 seconds without pool replies; recovery passed |
| Maintenance | Oversized OTA rejected before flash writing; stayed off until explicit resume; guarded restart and fresh public-pool share passed |

The fallback run `fallback/20260918T171526.570323Z` contains four passing
cases followed by an inconclusive backpressure attempt, so its overall
status is failed. Each successful protocol/fallback run restored settings.
The candidate was also verified on the original pool at about 1.58 TH/s.

Independent SHA256d checks validated 44 V1 shares across 40 jobs without
failures or duplicates. One raw-frame job lacked the payload needed for
independent reconstruction. Four V2 standard shares were verified using
recorded submissions and pinned fixture constants. V2 extended transcripts
lacked the extranonce needed for a separate complete hash reconstruction.

Tuning evidence consists of sampled firmware readbacks, not electrical
transient measurements. Resume API calls took about 31.7 seconds; the
WebSocket monitor reconnected during the wait. Pause calls took 0.16–0.51
seconds. These measurements do not establish continuous HTTP responsiveness
during startup.

## Blocking failure: sustained TCP backpressure

Run `backpressure-rerun/20260918T173130.700668Z` stopped pool socket reads
while continuing to send work. The listener used a small receive buffer and
window clamp. Host `TCP_INFO` confirmed a zero advertised receive window
starting at 110.74 seconds. After 32 zero-window samples, the miner entered
SAFE_OFF at 143.67 seconds, about 33 seconds after the first zero window.

The device reported:

```text
hardware_fault: parser error counter dropped_results increased baseline=0 curre
```

The message is truncated by the firmware. Bridge FIFO and software-ring
overflow counters remained zero. Device logs recorded repeated 5000 ms
transport write timeouts, result processing latency up to 12.6 seconds, and
verified OFF_SAFE shutdown. Fan/temperature logs continued during the blocked
writes. The test failed, and cleanup restored pool settings but reported an
error because the latched fault prevented mining from resuming.

Source inspection supports this failure chain:

1. `ASIC_result_task` calls `stratum_submit_share()` synchronously.
2. V1 submission calls `esp_transport_write()` with a 5000 ms timeout.
3. Independent BZM polling continues to enqueue raw results in the
   256-entry queue in `bzm_transport.c`; the same blocked result task must
   consume them.
4. Queue overflow increments `dropped_results`. Runtime health rejects that
   counter change, and the power owner shuts down and latches the fault.

Result consumption therefore needs isolation from network writes, with
bounded submission buffering and explicit stale-session/overflow handling.
The existing parser health interlock must remain meaningful. This run did
not compare the old firmware under the same fault injection, so it does not
establish when the defect was introduced.

There is also a health-reporting mismatch: the top-level `hardware_fault`
was set while `asicHealth.lastFaultCode` remained zero and
`userActionRequired` was false. Tests must inspect both surfaces until fault
publication is reconciled.

## Fixture corrections and recovery

The first negative OTA probe sent an empty body with an oversized declared
length and received HTTP 408 before reaching the intended rejection. The
corrected probe sent one byte with that oversized length; it reached the
OTA-size rejection and passed the maintenance checks without writing flash.

The first 45-second backpressure attempt did not reach a zero window and
failed its fixture assertion. Extending it to 180 seconds exposed the real
device failure above. An intermediate rerun discovered no tests because of
the local wrapper's module identity; it performed no hardware test. These
attempts remain in the local artifacts and are not counted as passes.

Recovery selected the preserved `ota_0` partition. `rollback-health.json`
records the original version, matching identity/settings hashes, unpaused
MINING, four ASICs, 944 engines, 1200 MHz / 3000 mV, no fault, and four fresh
accepted shares. A later `rollback-final-health.json` check confirmed 36
accepted shares, about 1.45 TH/s, and unchanged settings with no fault.
The test pool listeners were confirmed stopped.

Still untested physically: cold power-on, interruption during startup,
forced overheating/cooldown and its interruption by pause or maintenance,
power-owner watchdog expiry, bridge firmware/SWD recovery, and representative
Bitmain boards. The backpressure failure must be fixed and rerun before
claiming continued result processing through slow network operations.
