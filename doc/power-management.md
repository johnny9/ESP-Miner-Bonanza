# Common board power management

`POWER_MANAGEMENT_task` owns mining startup, shutdown, voltage/frequency
changes, pause/resume, pool-loss recovery, overheating, and maintenance for
both Bitmain and Bonanza boards. `main/bzm_controller.c` is replaced by board
operations under `main/power`; it no longer creates a separate mining stack
or runs a separate tuning/recovery policy.

## Ownership and boot

`main.c` still establishes board identity, the initial reset state, and
peripherals before starting tasks. It creates the same job, result, hashrate,
statistics, and Stratum tasks for every board. Hardware startup is admitted
only after all required tasks exist. A task-creation failure leaves startup
disabled while HTTP recovery remains available. Self-test uses the common
startup/shutdown owner, but remains a separate workload; this change does not
qualify Bonanza factory self-test.

`components/power_management/power_policy.c` implements the portable policy.
`main/power/board_power.c` selects the board operations. The Bitmain operations
retain ASIC initialization, UART recovery, and power/reset sequencing;
`bzm_board_power.c` retains Bonanza's staged bring-up, bridge/regulator
readbacks, bounded PLL changes, and verified OFF_SAFE shutdown.

| Event | Owner behavior |
| --- | --- |
| Boot or resume | Validate startup before publishing the running state |
| Settings change | Raise voltage before raising clocks; lower clocks before lowering voltage |
| Pause | Revoke BZM work authorization immediately, then complete board shutdown |
| All pools unavailable | Stop without changing user pause intent; restart after pool recovery |
| Overheat | Stop, keep full fan, require valid cooldown evidence, save reduced settings, then restart |
| Hardware fault | Stop and latch the fault; a healthy sample alone cannot restart mining |
| OTA, bridge update, restart | Require board shutdown and exclusive maintenance ownership |

Cooling requires at least 30 seconds and a valid regulator temperature at or
below 95 C. Boards with an available powered-down ASIC sensor also require a
valid reading at or below 45 C; a hotter reading restarts the cooldown timer.
Recovery reduces the saved target by 100 mV and 100 MHz, bounded by the board
limits. User pause, pool loss, and maintenance can interrupt cooling without
losing their intent. Overheat detected during startup enters the same cooling
path. Other startup failures remain faults.

## Requests and independent I/O

HTTP, BAP, BLE restart, and Stratum restart paths request operations through
the power-task mailbox. They do not perform lifecycle or tuning operations.
Requests retain their storage until both the caller and owner release it.
Timed-out requests revoke work; a delayed resume cannot erase a newer stop.
A timed-out maintenance acquisition is released if it subsequently succeeds,
and a queued release still executes after its caller times out. A failed or
completed maintenance operation leaves mining paused until explicit resume.

Bonanza's `bzm_io` task handles transport polling, bridge heartbeat, and
runtime telemetry independently of network submission. It can revoke work
and withhold lease renewal on bad health, but power management decides and
executes shutdown or recovery. The power owner renews a separate watchdog;
the I/O task cannot keep an unresponsive owner alive indefinitely. Bounded
startup transactions retain their existing internal lease servicing.

The result task copies validated pool shares into a 32-entry queue without
waiting for space. A common Stratum submission task owns network writes for
all ASIC families. Queue entries own their result, job ID, extranonce, and
protocol/session metadata. Full queues drop new network submissions while
local hash accounting and ASIC receive processing continue. Drops are logged
at a bounded rate; they do not disable parser health checks.

Generation reads use an atomic snapshot and never acquire the socket mutex.
The sender rechecks pool/session identity and generation under that mutex
before writing; clean jobs and reconnects retire queued generations. Failed
or partial frames close the affected socket so its receive owner reconnects.
Send timing and pending-share accounting reflect completed writes, not queue
acceptance. The [share-submission report](share-submission.md) records the fix
and hardware validation of the synchronous-submission failure.

Fan I/O and bridge-info queries serialize with board initialization and
startup/shutdown. They are excluded during maintenance. Ordinary power
sampling and Bonanza polling are also suspended while an updater owns the
board; this prevents interference with SWD recovery. The blank-bridge
exception still requires independent regulator-off readback.

## Upstream interface compatibility

Reset keeps upstream's `asic_reset(low_ms, release_ms)` and
`asic_hold_reset_low(void)` entry points. Boot selects the GPIO or bridge
backend once, after board identification and before the first reset operation.
Callers do not pass `GlobalState`; bridge failures propagate without falling
back to GPIO. GPIO retains caller-specified timing, while the bridge retains
its existing board-specific pulse timing.

`VCORE_get_voltage_min_mv(GlobalState *)` also retains upstream's contract.
The common power policy gets its voltage floor from the effective regulator
configuration, including Bitmain NVS overrides, and rounds up after dividing
by the voltage-domain count. Bonanza retains its fixed board profile.
Non-TPS546 boards return zero; the policy retains its existing 1000 mV floor
for those boards.

`TPS546_write_entire_config(void)` retains its public void signature and the
configuration's switching-frequency field retains its upstream `uint16_t`
type. An internal checked operation lets `TPS546_init()` propagate failures.

The common lifecycle request/ownership contract remains a shared change.
The existing [common-job and result migration](asic-common-interface.md)
also remains; this cleanup does not restore legacy heap-owned Bitmain jobs.
These changes must be reviewed separately from board configuration and
backend selection, which do not require changing existing Bitmain callers.

## Validation

The subsequent [master integration report](power-management-master-integration.md)
records the merge with PR #1972 and validation of those merged sources. The
results below describe the original power-management candidate.

Local validation on 2026-09-18 with ESP-IDF 6.0.2, based on `84765393`:

- 422 native tests pass under GCC and Clang with address, undefined-behavior,
  and leak sanitizers; the inventory check passes.
- 527 QEMU tests pass, including the actual FreeRTOS power task with link-time
  board ports. The fixture covers caller timeouts, one-task ownership,
  maintenance exclusion, abandoned acquisitions, and queued releases. Submission
  tests exercise the actual FreeRTOS queue and worker during a blocked write,
  queue saturation, stale work, and generation reads while the socket is locked.
- Reset tests exercise backend selection, timing argument forwarding, and
  failure propagation. QEMU voltage-limit tests exercise the production
  configuration getter with NVS overrides, multi-domain rounding, and the
  fixed Bonanza profile; they perform no physical reset or power I/O.
- Firmware builds using the existing frontend bundle; frontend sources are
  unchanged.
- Native coverage gates pass: 78.6% line and 65.7% branch coverage across
  instrumented production files. The new portable policy has 96.8% line and
  82.2% branch coverage; reset dispatch has 100% line and branch coverage.
  The host suite instruments 55 of 136 eligible production files. The hardware
  adapters are outside native coverage.

The first candidate faulted under sustained TCP backpressure; its
[historical hardware report](power-management-hardware.md) preserves that
failure and recovery. The corrected candidate passed both measured socket-stall
cases without receive overflow or safety faults, reconnecting with fresh
accepted shares. All 26 V1/V2 protocol cases also passed. See the
[share-submission report](share-submission.md) for exact provenance, additional
hardware results, and remaining qualification limits. Native/QEMU results do
not establish untested physical properties or qualify Bitmain hardware.
