# Nonblocking share submission — 2026-09-18

The shared result task no longer writes to pool sockets. It copies validated
shares into a fixed 32-entry queue; a common Stratum worker submits them for
V1, V2 standard, and V2 extended mining. This applies to every ASIC family
without changing the common ASIC job or driver contracts.

Queued shares own their result, job ID, extranonce, and generation metadata.
The worker checks generation before dispatch, and each protocol checks the
session, pool, generation, and applicable version mask under the transport
mutex immediately before sending. Generation reads in result processing and
job creation are atomic and do not wait for that mutex. The connection supplies
the username, so queued shares never borrow mutable pool credentials.

A full queue drops new network submissions with a bounded warning rate.
Local nonce validation and accounting continue. Parser overflow remains a
hardware fault; its health check has not been weakened. Failed or partial
writes shut down the affected socket so the existing receive owner reconnects.
Only complete writes update send timing and pending-share accounting.

This addresses the parser queue overflow in the
[initial power-management hardware report](power-management-hardware.md).

## Provenance

- Candidate: `bzm-pm-424e8c3ad032`, Bonanza 1002, four ASICs / 944 engines.
- Base: `8476539327e5aa723615efb2cc411adb8acfa1f2`, plus archived uncommitted changes.
- Source digest: `424e8c3ad03203812be05ec9c9fb70e2039a04bae2c9630e3a4c04cc1511b7da`.
- Binary SHA-256: `407eb9ca0c9bd4fca2f6187f1c9633c825bb4c1c4ae84a40a120f276fff1d056`.
- Application size: 2,891,120 bytes; built with ESP-IDF 6.0.2.
- Firmware archive: `.cache/share-hardware/20260918T175700Z/`, including source,
  submodule contents, frontend bundle, configuration, binary, ELF, and logs.
- Testcode: `ec36db41270aad3e665237bf003046d8bc17e50e`, plus preserved local fixtures.
- Hardware evidence: sibling `mining-qa-bzm-power-hardware/artifacts/share-submission/`.

Testing used direct LAN HTTP/OTA, WebSocket logs, and local pool servers.
SSH authentication was unavailable; no serial or physical power switching was
used. The original `bzm-selftest-713420fc` image remains in `ota_0`.

## Software validation

422 native tests passed under GCC and Clang with address, undefined-behavior,
and leak sanitizers. All 527 QEMU tests passed. Coverage gates passed: 78.6%
line and 65.7% branch coverage within instrumented production files; 55 of
136 eligible files are instrumented. Firmware compilation also passed.

New tests cover owned snapshots after producer-buffer reuse, metadata bounds,
queue saturation during a blocked write, generation reads while the transport
mutex is held, stale queued work, failed-send timing, and incomplete V1/Noise
writes. Queue saturation is proven by the actual FreeRTOS queue/worker test;
the hardware runs below did not fill the submission queue.

## Backpressure validation

Run `backpressure-confirmed/20260918T181210.487556Z`: **2 passed, no failures or
cleanup errors**. Both cases paused server reads with a small TCP receive
window, continued sending jobs, required a measured zero window and a write
timeout, and verified reconnection plus fresh accepted shares afterward.
The first retained the original frequent clean-job workload; the second kept
older work valid to sustain more results. The sender reconnects during each
observation; later zero-window samples still describe the old stalled socket.

| Observation | Frequent clean jobs | Older jobs remain valid |
| --- | --- | --- |
| Stalled-read observation | 180.08 s | 180.35 s |
| First zero receive window | 110.60 s | 16.68 s |
| Zero-window samples | 65 | 150 |
| Additional locally valid results | 696 | 3,954 |
| Maximum sampled system-info latency | 0.134 s | 0.349 s |
| Safety faults / bridge overflow | None | None |
| Reconnection and fresh accepted shares | Passed | Passed |

After both cases, an independent check verified the original settings and
pool hashes, 1200 MHz / 3000 mV, four ASICs / 944 engines, fresh public-pool
acceptance, about 1.46 TH/s, and no fault.

The initial fixture attempt ended its first stall too early to observe a
write timeout/reconnection. Its proposed difficulty-16 case could not increase
submission rate because the firmware clamps it to 256. Its loader also
selected inherited cases; that run was stopped after three completed
lifecycles, at an independently verified cleanup boundary. The corrected run
selected exactly two cases and allowed a bounded observation extension.
These initial attempts are preserved as incomplete fixture evidence, not
counted as passing backpressure tests.

## Protocol validation

| Suite | Result | Run |
| --- | --- | --- |
| V1 | 12 passed | `sv1/20260918T182342.629778Z` |
| V2 standard | 7 passed | `sv2-standard/20260918T182817.227815Z` |
| V2 extended | 7 passed | `sv2-extended/20260918T183010.140202Z` |

All three suites verified cleanup with no errors. Independent SHA256d checks
verified 42 V1 shares across 40 jobs, with no failures or duplicates, and 8 V2
standard shares across 4 jobs. The latter reconstruction uses the pinned job
fixture constants as well as captured submission fields. One V1 raw-frame job
lacked a captured payload, and the V2 extended transcript lacks extranonce
material needed for a separate complete hash audit.

## Pool fallback and independent I/O

Run `fallback/20260918T183355.704171Z`: **4 passed, no failures or cleanup
errors**. Manual pool selection, automatic failover/recovery, recovery through
either pool after both were unavailable, and a short pool silence all passed.
The two-pool outages reached `SAFE_OFF` with core voltage below 200 mV while
preserving the unpaused user intent. Fresh accepted shares confirmed recovery.
During the short silence, 15 telemetry samples recorded continued mining,
156 additional locally valid results, and no bridge overflow.

An earlier fallback attempt (`20260918T183209.227208Z`) ran immediately after
the V2 suite restarted the miner. All four cases rejected the initial health
precondition while hashrate was still warming up; no test bodies ran. The
successful rerun used the same cases after an independent health check
confirmed the original settings, normal hashrate, and fresh public-pool
acceptance. Both runs are preserved.

The two backpressure cases, 26 protocol cases, and four fallback cases total
32 passing network hardware cases on this candidate.

## Power lifecycle

The local lifecycle fixture exercised the production HTTP control path:

- Downward tuning sampled 1200, 1175, 1150, 1125, and 1100 MHz at 3000 mV,
  followed by 1100 MHz at 2950 mV. Upward tuning sampled increased frequency
  only at 3000 mV and returned to 1200 MHz with fresh valid results.
- Two pause holds reached `SAFE_OFF`, core voltage below 200 mV, stable result
  counts after settling, and a queryable bridge. Both resumes returned to the
  original target and produced fresh valid results without a reboot.
- A one-byte OTA request declaring more than the 4 MiB partition size was
  rejected by the size check before `esp_ota_begin()`. The maintenance guard
  released ownership and left the miner safely paused until explicit resume;
  neither application image was erased.
- Resume after maintenance and a guarded restart both returned to the original
  target with fresh valid results. The restarted miner also received fresh
  public-pool acceptance. Lifecycle cleanup completed without errors.
- A separate run paused an active downward tuning ramp, held `SAFE_OFF`, then
  resumed at the original target with fresh valid results. Its cleanup also
  completed without errors.

Evidence is in `lifecycle/` and `ramp-cancel/`, including per-second samples,
device logs, and the completed phase lists in each `result.json`.

During this sequence the WebSocket collector reported an `InterfaceError`
and switched to API polling. API control and health checks continued; this
run does not establish uninterrupted WebSocket delivery across lifecycle
operations. The warning remains in `lifecycle/lifecycle.log`.

## Final device state

At 18:56:17 UTC, `final-health.json` confirmed candidate
`bzm-pm-424e8c3ad032` running in `ota_1`, matching device identity and original
settings/pool hashes, unpaused `MINING`, 1200 MHz / 3000 mV, four ASICs / 944
engines, no safety fault, and a newly accepted public-pool share. The sampled
hashrate was 0.91 TH/s shortly after resume; the earlier uninterrupted health
check measured 1.46 TH/s. The validated candidate remains running, with the
original `bzm-selftest-713420fc` preserved in `ota_0` for rollback.

The test listeners on ports 4333 and 4334 are stopped. The consolidated
`validation-summary.json` includes all successful runs and the initial fixture
and warmup failures. `local-test-sources.json` hashes the preserved fixtures
and helper scripts. These evidence files are local, ignored artifacts; no
external report was published.

## Scope and remaining qualification

The measured TCP-stall failure is resolved by these runs. A full submission
queue can intentionally lose network shares while keeping receive processing
alive; its capacity and drop behavior are tested in QEMU, not observed under
the hardware workloads above. V2 submission and incomplete Noise writes are
covered, but the physical zero-window injection uses V1.

Cold power-on, interruption during startup, forced overheating/cooldown,
power-owner watchdog expiry, bridge firmware/SWD recovery, and representative
Bitmain boards remain untested physically. The separate fault-publication
mismatch documented in the initial report is unchanged; tests inspect both
the top-level hardware fault and ASIC health where applicable. These results
do not constitute complete board or upstream hardware qualification.
