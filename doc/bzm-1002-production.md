# Bonanza Bitaxe 1002 production support

Board 1002 uses an automatic production profile. After Wi-Fi and the
normal Stratum queue are ready, the ASIC driver transitions through:

```text
SAFE_OFF -> STARTING -> MINING -> FAULT or MAINTENANCE
```

There is no manual arm, numeric startup level, operator heartbeat, or remote
bring-up action. The controller first proves safe-off, verifies a compatible
bridge, enables and verifies the 2.800 V startup TPS546 profile, discovers four
ASICs, configures telemetry and both 800 MHz PLLs, activates 944 engines with
bounded stack skew, and then starts the normal ESP-Miner work, result,
statistics, Stratum, and live-frequency tasks.

## Work scheduling

Each of the 236 logical engines receives an independent header/extranonce and
uses the full 32-bit nonce domain. Only the four ASIC instances of that one
logical engine divide the nonce range. Per-engine current and previous
generations remain attributable through enhanced-mode sequence identifiers.
The timestamp budget is 60, matching the BIRDS production scheduling model.
The ASIC-facing PIO link remains at the qualified 5 Mbaud rate. The separate
raw bridge-to-ESP UART runs at 2 Mbaud, more than twelve times its 160 kbit/s
measured receive payload budget, to provide board-level signal margin without
reducing ASIC work coverage. Cumulative PIO FIFO and DMA-ring overflow counters
are read through the bridge control protocol.

## Safety and recovery

The RP2040 bridge owns an independent short output lease. ESP-Miner services
that lease during startup, work programming, and continuous monitoring. A
persistent bridge, fan, TPS, telemetry, PLL, parser, attribution, or dispatch
fault closes work dispatch and proves safe-off. ESP/AxeOS OTA, ordinary bridge
firmware updates, and restart acquire the same verified-safe maintenance
boundary before proceeding. A factory-blank, nonresponsive bridge is the sole
exception: ESP-Miner keeps the independently controlled TPS546 disabled,
verifies PGOOD low and VCORE discharged, and permits only exclusive bridge SWD
recovery. Mining remains disabled because bridge-owned reset, 5 V, fan, trip,
and lease state cannot be verified.

Parser byte realignment and result-attribution recovery are bounded production
behaviors. Their byte, event, rejection, and timeout thresholds are the only
Bonanza recovery settings exposed in Kconfig. Operators may select a frequency
target from 800 through 2000 MHz; direct voltage tuning is not exposed.

## Live frequency targeting

Startup always proves the complete mining path at 800 MHz. If the configured
target is higher, the controller keeps TDM and normal pool work active while it
applies the BZMD-derived initial shortcut, capped at 1425 MHz, then waits 90
seconds for thermal stabilization. It advances in 25 MHz steps and qualifies
every ASIC/PLL domain from attributed valid and rejected mining results. A
domain that cannot sustain a step is returned to its last passing frequency
while the remaining domains may continue.

Changing the AxeOS target while mining starts the same process without an ESP
restart. Downward changes use bounded 25 MHz steps. The frequency policy derives
the TPS546 rail command, verifies each change against live readback, and may
retry a failed qualification at the next policy voltage. The command is always
capped at 3.20 V; a voltage or PLL transaction that cannot be verified latches
the normal fail-closed supervisor path.

## AxeOS status

The normal `/api/system/info` response includes optional `asicHealth` data.
AxeOS renders lifecycle and state age, current pool/work age, warm-up and
average hashrate, four-ASIC and 944-engine status, the active clock and voltage,
measured VOUT, measured ASIC-rail power, hottest fresh ASIC temperature, fan
telemetry, bridge compatibility, recovery
counters, and the last persistent fault with a safe next action.

Qualification snapshots, raw templates, midstates, per-engine traces, and
manual startup controls are intentionally absent from production firmware.
The production bridge updater accepts a separately supplied raw RP2040 image,
requires its embedded BZM bridge identity manifest by default, programs and
read-back verifies it over the onboard SWD connection, resets the bridge, and
confirms that the running protocol and version match the manifest before
completing. The manifest identifies board 1002, the bridge image kind, protocol,
and build-derived version, with a CRC-32 protecting the manifest from accidental
damage. It is an identity guard, not a cryptographic signature.

A blank bridge no longer causes an ESP panic/reboot loop. Wi-Fi, AxeOS, and the
bridge firmware HTTP endpoints remain available while the controller reports a
fault and refuses to mine. After flashing the bridge, restart ESP-Miner so the
normal complete safety and compatibility checks run before mining.

The opt-in hardware regression reproduces this recovery cycle without adding a
production erase endpoint. It builds a 266-byte nonresponsive test image from a
known-good bridge binary by preserving RP2040 boot2 and branching forever
before UART or GPIO setup. That deliberately unidentified image is uploaded with
the test-only `force=true` query argument. Force bypasses only manifest identity;
RP2040 structure, supported-board, safe-off ownership, SWD read-back, and
post-flash protocol checks remain mandatory. The script verifies the board,
ASIC, PSRAM, device MAC, USB serial MAC, current bridge version, known-good
manifest, and recovery image before changing the bridge. If a later assertion
fails, it still attempts to restore the known-good image:

```bash
. ~/esp/esp-idf-v5.5.3/export.sh
python3 tools/bzm_blank_bridge_recovery_regression.py \
  --device BITAXE_IP \
  --interface LAN_INTERFACE \
  --serial ESP_USB_SERIAL_DEVICE \
  --expected-mac ESP_MAC_ADDRESS \
  --bridge-firmware /path/to/bonanza-bridge-fw.bin \
  --expected-bridge-sha256 EXPECTED_SHA256 \
  --expected-bridge-version EXPECTED_VERSION \
  --confirm ERASE-BRIDGE-FIRMWARE
```

## Bridge compatibility

| ESP-Miner Bonanza controller | Required bridge protocol | Result |
| --- | --- | --- |
| Production MVO | protocol 1.0 | Mining allowed with raw RX bytes and the RX-stats command |
| Production MVO | protocol missing, major mismatch, lease/trip policy missing, or required control path missing | Safe-off with an incompatible-bridge fault |

Before VCORE can be energized, the bridge must report the fixed trip-latch
policy plus coherent lease, reset, 5 V, full-fan, sampled-trip, and fault
state. The ESP then independently verifies live fan tach, the TPS command,
PGOOD, measured VCORE, power status, and temperature. Legacy bridge capability
fields for hardware-independent cutoff and tach interlock remain truthful but
are not treated as implemented on board 1002.

## Verification

Run the host/QEMU tests, frontend tests, and complete ESP-IDF build before an
OTA. A device regression must then verify automatic mining, independent work
across all logical engines, pool job rotation, live AxeOS updates, bounded
parser recovery, and verified safe-off before restart or maintenance.
