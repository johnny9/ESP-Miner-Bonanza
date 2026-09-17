# PR #1972 integration — 2026-09-17

Merged [upstream PR #1972](https://github.com/bitaxeorg/ESP-Miner/pull/1972)
through `492392e677d7d208d85f8898ad3aad00f79d9e94` into Bonanza master at
`8476539327e5aa723615efb2cc411adb8acfa1f2`.

## Integration decisions

- All five Bitmain drivers accept the borrowed common job and encode the final
  wire packet directly. BM1366/68/70/73 share the PR's encoder; BM1397 generates
  its one or four software midstates in its final packet. The old `bm_job` type,
  builder, and conversion wrappers are removed. Bonanza retains its allocation-free
  submission and value-copy store instead of upstream's heap-backed active slots.
- Bitmain decoders copy the matched common job and provenance under the existing
  store lock. BZM captures the same information while mapping its engine result.
  The event carries the snapshot through validation, duplicate detection and pool
  submission. Result handling has no job-store dependency. Physical slot IDs stay
  in the Bitmain adapter; the common event retains Bonanza's opaque work handle.
- Session retirement remains authoritative before pool submission and accounting.
  Decoded results survive ordinary slot replacement, while clean jobs and retired
  pool generations remain stale. BZM's engine assignments, timestamp reconstruction,
  flush barriers, duplicate-share filtering, and controller lifecycle are preserved.
- Common-job layout, borrowed `void ASIC_send_job` contract, backpressure and retry
  behavior remain unchanged. Submission rejects unterminated metadata before driver
  dispatch. Coinbase allocation failures retain Bonanza's stricter behavior:
  reject the build and leave its destination untouched.
- The PR's optimized byte-safe hash helpers and explicit Bitmain counter conversion
  are included. Bonanza's existing portable midstate helper remains shared with BZM,
  and its normalized hashrate path is preserved. Sparse-mask carry/wrap was already
  fixed in Bonanza and now also has the PR's packet/result vector regressions.
- QEMU retains Bonanza's larger 16 KB test stack and enables the PR's stack-end
  watchpoint. Tests share the fork's existing host/QEMU fixtures, adapting upstream
  pointer-ownership assertions to its value-copy store.
- The PR's rebased upstream history also brings the local-domain swarm fixes,
  mDNS 1.12.0 pin, and regulator minimum-voltage clamp.

## Validation

- 411 native tests pass with GCC and Clang, with address, undefined-behavior,
  and leak sanitizers enabled.
- 509 ESP32-S3 QEMU tests pass, including Bitmain result ownership for every
  driver/protocol, BZM snapshots after retirement, exact packet bytes, sparse-mask
  wrap, disabled rolling, invalid response IDs, and result tasks without a store.
  Five hardware/device exclusions remain outside QEMU.
- Test inventory, all 17 inventory/coverage tooling tests, and Bitaxe 1002 factory
  configuration checks pass.
- Existing coverage gates pass: 77.8% lines and 65.2% branches within instrumented
  files; 55 of 132 eligible first-party files are instrumented. SV1 protocol remains
  at 100% line/function and 92.4% branch coverage. These are not firmware-wide
  coverage percentages.
- All 109 AxeOS browser tests and its production build pass.
- ESP-IDF 6.0.2 builds the ESP32-S3 application with the newly built AxeOS assets;
  the application has 31% partition space free.

Local logs are in `.cache/pr1972-validation/`, with coverage reports under
`build/host-coverage/coverage/`. Firmware is in `build/firmware/`.
No miner was flashed and no new physical mining run was performed for this merge.
