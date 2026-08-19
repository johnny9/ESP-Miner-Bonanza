# Stratum session and message safety — risks and scope

## Scope and review unit

- **In:** Socket policy, V1/V2 setup/order, message bounds, ownership, timing,
  crypto/transport failures, reconnect cleanup, public protocol state, tests.
- **Out:** Pool selection/failover/client-task lifecycle, pool economics, and
  ASIC-specific work encoding after template handoff.
- **Smallest coherent change:** A protocol feature should update only its parser,
  session, contract, and focused tests; unrelated network/refactor changes split.
- **Split out:** Scoreboard presentation and general settings transactions.

## Assumptions and open questions

- Current 180-second receive and 5-second send timeouts are policy constants;
  any change needs failover and slow-pool evidence.
- Define behavior on systems without PSRAM before expanding V2 availability.

## Failure modes

| Failure | Impact | Detection | Mitigation |
|---|---|---|---|
| Oversize count/frame | Overflow or memory exhaustion | Maximum+1 parser cases | Validate before copy/allocation |
| Blocked receive | Failover never occurs | Silent-pool test | Socket receive timeout |
| Stale session state | Wrong work/crypto context | Reconnect case | Full close/reset boundary |
| Response by order | Wrong latency/share outcome | Out-of-order IDs/sequences | Correlate actual identifier |
| Ignored crypto/allocation error | Corrupt/insecure session | Failure injection | Abort and clean up |

## Security, resources, and rollout

- Pool traffic is untrusted. Bound every length/count and authenticate Noise/TLS
  according to configured mode without leaking secret material.
- Track internal/PSRAM use, fragmentation, task stack, queue depth, frame size,
  and reconnect rate.
- Rollback must preserve NVS protocol/channel representation and clear any
  session state by reboot or explicit close.
