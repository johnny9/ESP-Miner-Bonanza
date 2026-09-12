# ESP-Miner upstream integration, 2026-09-12

This branch integrates the 32 upstream commits after `c32b52db` through
`1df7ba1ec12833045693cf1352042e8d78628c22` (2026-09-11), on top of Bonanza
`588246bd52571953ff117408541a0309bd7a2f15` (PR #4).

## History

PR #4 carried the previous upstream integration as linearized history. Its
head `de3fa146c533e3ea48167363ecd85434b713d6dc` has the same tree as the original
integration commit `46009c9c3fb3e67e134631696c8ee5b688b9f0f6`, which already
included `c32b52db`. An ancestry-only merge records that existing integration
before the new upstream merge. It makes no source changes. Preserve these
merge parents when integrating this branch so future upstream merges start
from the correct common ancestor.

## Integration decisions

- Adopt upstream's unified Stratum task, clients, pool switching, and pooled
  `miner_job_t` work. Convert each dispatched job to Bonanza's owned
  `mining_template_t` and job-store snapshots; retain pool ID, protocol,
  extranonce, final ntime, and version data through submission. Recheck pool
  identity under the transport lock. Respect the ASIC's supported SV1 version
  mask and keep BZM's driver-managed work refresh and rolling.
- Keep Bonanza's staged startup, shutdown guards, bridge updates, board 1002
  pins, external display, and tuning controls. Retain its verified TPS546
  absolute limits; upstream's configurable voltage ratios apply only to
  ordinary profiles.
- Port the new BM1372/BM1373 driver into Bonanza's driver table and owned work
  interface. Retain upstream's Naja Duo/Gamma Hex pin and color-display data.
- Incorporate upstream's Angular control flow, charts, pool editing, mock
  environment, update progress, SNTP, cumulative statistics, and connection
  fixes while preserving Bonanza's health, tuning, and bridge UI.
- Move the factory configuration to `configs/config-1002.csv` alongside
  upstream's renamed configurations. Keep the release matrices scoped to
  board 1002.
- Track actual coinbase buffer capacities. SV1 and SV2 reject writes beyond
  smaller buffers in environments without PSRAM. Resetting a pool slot keeps
  its allocated buffers and capacities. Failed coinbase allocation prevents
  work dispatch.
- Increase the test application's stack to 16 KiB for the larger protocol
  fixtures. QEMU checks heap integrity after each test; the former 3.5 KiB
  stack corrupted heap metadata during nested template construction.

## Validation

Local checks use ESP-IDF v6.0.2 and Node v24.14.0:

- Board 1002 factory configuration validator: passed.
- Python tool regressions: 37 passed.
- Axe-OS headless tests: 99 passed.
- Axe-OS production build and OpenAPI generation: passed.
- QEMU ESP32-S3 component suite: 385 tests, zero failures. Tests tagged
  `not-on-qemu` remain excluded. New cases cover all three pool-slot protocol
  adapters, owned metadata and pool IDs, zero-difficulty submission, and
  coinbase capacity rejection; existing Bonanza regressions remain included.
- ESP32-S3 firmware build with the production web bundle: passed.

## Hardware qualification still needed

This branch has not been flashed to a miner. Before promoting it to a release,
verify Bonanza cold boot and staged startup, power/thermal behavior, fresh
accepted SV1 shares, SV2 standard and extended shares, pool edits and fallback
recovery, and bridge/external-display operation. QEMU does not establish
physical mining or power-rail correctness.

The workspace's pool-fallback regression profiles use `192.168.1.214` on TCP
4333 and 4334; only one suite should own those listeners at a time. Confirm
fresh miner connections and shares when running those checks.
