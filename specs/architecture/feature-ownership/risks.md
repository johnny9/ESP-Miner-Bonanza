# Feature ownership and dependency direction — risks and scope

## Scope

### In

- Feature/module ownership, portable contracts, ports/adapters, dependency
  direction, composition, boundary enforcement, and migration sequencing.

### Out

- Rewriting all current code in one change, selecting new product behavior, or
  requiring identical optional capabilities across hardware backends.

### Review unit

- **Why this is the smallest coherent change:** The architecture contract and
  affected feature boundaries must agree before implementation can migrate one
  seam at a time.
- **Work intentionally split out:** Each feature/backend migration, behavior
  correction, and hardware qualification remains a separate change.

## Assumptions

- Existing behavior can be wrapped temporarily while policy is extracted.
- Standard-C contracts are sufficient for the core work, result, capability,
  status, and self-test decision data.

## Open questions

- Which composition state remains centralized, and which current `GlobalState`
  fields move into owned feature contexts first?
- Should architecture checks use a source include allowlist, CMake component
  graph, or both?

## Failure modes

| Failure | Impact | Detection | Mitigation or recovery |
|---|---|---|---|
| Big-bang rewrite | Unreviewable behavior/safety regressions | Diff and commit dependency audit | Migrate one seam with compatibility wrappers |
| Port mirrors concrete backend | Switches move but coupling remains | Fake-backend and second-backend review | Model feature needs, not existing function catalog |
| Descriptor becomes mutable state bag | New `GlobalState` under another name | Ownership review | Immutable facts only; runtime snapshots separate |
| Optional failure treated as unsupported | Hidden hardware fault | Failure-injection tests | Explicit capability plus typed operation result |
| Duplicate old/new path persists | Divergent behavior | Call/dependency graph check | Dated removal criterion per migration |
| Host tests replace HIL claims | Unsafe board release | Evidence audit | Keep unit, QEMU, and hardware evidence distinct |

## Ownership and boundary risks

- **Shadow or duplicated state:** Compatibility phases can temporarily duplicate
  paths; they require one authority and a removal condition.
- **Allocation, pointer, callback, or task lifetime:** Ports must state context,
  command, event, and snapshot ownership before adapters are implemented.
- **Untrusted input and copy/parse bounds:** Validation stays with the feature or
  protocol boundary that first interprets the input.
- **Blocking, race, lock, queue, timeout, or reconnect behavior:** Platform
  adapters expose bounded operations; domain policy must not assume callbacks
  are synchronous or resources immortal.
- **Cross-layer drift:** Feature specs and architecture checks must change with
  any intentionally altered dependency direction.

## Security, privacy, and safety

- Abstraction must not bypass safe-off, validation, authentication, or secret
  handling. Typed ports make those preconditions explicit rather than optional.

## Performance and resource risks

- Extra contexts/events/snapshots consume memory. Prefer fixed/bounded storage
  and measure task stacks, heap, latency, and flash at migration checkpoints.

## Rollout and rollback

- Preserve working compatibility entry points until the new path is proven;
  rollback selects the prior composition without changing persistent data.
