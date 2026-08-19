# Stratum session and message safety — acceptance

## Functional and protocol behavior

- [x] **MINE-STRATUM-AC-01:** V1 sends suggested difficulty and optional
  extranonce subscription only after a successful matching authorization result.
- [x] **MINE-STRATUM-AC-02:** V1 response latency is correlated with the actual
  message ID; V2 submit latency is keyed by acknowledged sequence number/batch.
- [x] **MINE-STRATUM-AC-03:** Common socket setup applies finite send/receive
  timeouts and TCP_NODELAY to V1 and V2 transports.
- [x] **MINE-STRATUM-AC-04:** Counts and lengths are checked before array access,
  copy, allocation, or template construction, including coinbase outputs,
  Merkle branches, extranonce, and V2 frames.
- [x] **MINE-STRATUM-AC-05:** Custom TLS certificate input is non-null before
  `strlen`, and transport/Noise failures close the session rather than continue.
- [x] **MINE-STRATUM-AC-06:** The V2 frame cap is 8192 bytes and frame buffers
  use PSRAM with checked allocation and cleanup.
- [ ] **MINE-STRATUM-AC-07:** Closing a session destroys its transport/Noise
  state and clears queued jobs before reconnect.
- [ ] **MINE-STRATUM-AC-08:** Focused regressions cover short/long/malformed
  frames, maximum counts, allocation failure, blocking timeout, out-of-order IDs,
  authorization ordering, reconnect/reset, and V1/V2 status strings.
- [ ] **MINE-STRATUM-AC-09:** Protocol/session components compile without ASIC,
  board configuration, or `GlobalState` dependencies. Negotiated behavior uses
  an injected immutable mining capability snapshot.
- [ ] **MINE-STRATUM-AC-10:** Valid protocol jobs are adapted to neutral mining
  work at one boundary; the protocol layer neither owns ASIC job slots nor
  interprets hardware results.

## Verification evidence

| Evidence class | Exact command, case, revision, target, result, and date |
|---|---|
| Source reconciliation | Inspected V1/V2 tasks, shared socket, parsers, Noise, mining template, and coinbase decoder at `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f` — AC-01 through AC-06 present; V1 close omits transport destruction so AC-07 is unchecked, 2026-08-10 |
| Existing test surface | `components/stratum/test/` contains parser/mining/coinbase cases; not run in this documentation change. AC-08 remains unchecked. |
| Firmware/QEMU | Not run; no implementation changed. |
| Mining/soak/HIL | Not run; no pool/device operation was authorized. |

## Acceptance rule

Parser or session changes require adversarial boundary tests and reconnect
evidence; source presence alone is not sufficient for a changed risk boundary.
