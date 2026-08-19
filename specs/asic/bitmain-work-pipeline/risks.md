# Bitmain ASIC backend — risks and scope

## Scope

### In

- Bitmain driver/capability selection, init/detection, neutral work conversion,
  packet/slot ownership, UART result normalization, validation, and frequency
  callback boundary.

### Out

- BZM bridge implementation, pool/session state machine, frontend presentation,
  regulator implementation, and general thermal policy.

### Review unit

- **Why this is the smallest coherent change:** Packet send and result decode
  share the hardware slot/job-store identity; the driver/capability contract
  prevents either side from leaking model knowledge upward.
- **Work intentionally split out:** Protocol parser, power safety, and settings
  migration have their own feature specs and tests.

## Assumptions

- A board has one Bitmain model and the ASIC result task is the only caller that
  consumes borrowed static model results.
- Device configuration accurately describes count, cores, difficulty, and safe
  operating limits for the physical board revision.

## Open questions

- What per-model barrier or reuse interval proves that a delayed old hardware
  slot cannot alias newly stored work after clean jobs or ID wrap?
- Which driver operations are mandatory for all hashing families versus explicit
  optional capabilities?

## Failure modes

| Failure | Impact | Detection | Mitigation or recovery |
|---|---|---|---|
| Store mutex init fails but mining starts | Undefined lock/use and lost work | Init failure injection | Fail system/ASIC initialization closed |
| Wrong model/count/CRC | Invalid commands or false hashing status | Chip-ID/count vectors and device logs | Reject initialization and hold safe state |
| Packet endianness/layout drift | No shares or wrong hashes | Golden per-model byte vectors | Central builder plus model packet tests |
| Slot reused while old result is in flight | Share attributed to wrong job | Delayed-result/ID-wrap test | Flush/barrier and documented reuse rule |
| Missing driver operation | Silent no-op or repeated work rejection | Driver registration test and startup validation | Refuse driver activation |
| PLL/reset/timing change lacks target test | Unstable or damaged operation | Named-board startup/frequency evidence | Preserve proven order/bounds; rollback firmware |

## Ownership and boundary risks

- **Shadow or duplicated state:** ASIC model, count, capabilities, frequency
  limits, and work cadence must derive from device config/driver data once.
- **Allocation, pointer, callback, or task lifetime:** Templates are cloned into
  the store; raw model/event pointers are borrowed and serialized.
- **Untrusted input and copy/parse bounds:** Pool fields must be validated before
  this layer; UART frames and indices remain untrusted hardware input here.
- **Blocking, race, lock, queue, timeout, or reconnect behavior:** Job store is
  cross-task and locked; serial reads and transitions must have bounded timing.
- **Cross-layer drift:** Capability/config additions require tests and any API
  representation, but Stratum/UI must not invent their own model tables.

## Security, privacy, and safety

- Invalid UART or work data fails without out-of-bounds access. Frequency cannot
  override device limits, voltage policy, overheat priority, or safe-off state.

## Performance and resource risks

- Template cloning consumes heap per active slot; excessive cadence can fragment
  memory or overwrite work before results return. UART delays affect hashrate and
  need single- and multi-chip measurements.

## Rollout and rollback

- Roll out model changes first on the exact board/ASIC count with serial logging
  and a recovery image available. Reverting must preserve compatible frequency
  persistence and restore a known-safe initialization sequence.
