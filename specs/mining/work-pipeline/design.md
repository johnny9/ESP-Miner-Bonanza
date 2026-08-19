# Protocol-neutral mining work pipeline — design

## Components and responsibilities

| Component | Responsibility | Implementation pointer |
|---|---|---|
| Portable mining contract | Work/template/origin/purpose/handle/result/candidate/event types and ownership helpers | Current split: `components/stratum/include/mining.h`, `components/asic/include/asic_work.h`, `components/asic/include/asic_result.h`; target: `components/mining/` |
| Work source adapters | Convert validated V1, V2, or diagnostic inputs into owned neutral work | Current: `components/stratum/mining_template.c`, `components/stratum_v2/sv2_mining_template.c`, self-test inline JSON path |
| Mining work service | Typed queue, refresh policy, derivation, source generation, and ASIC submission | Current: `main/work_queue.c`, `main/work_queue.h`, `main/tasks/create_jobs_task.c` |
| Provenance store | Map pipeline work handle/generation to owned template and opaque origin | Current: `components/asic/asic_job_store.c`; target mining-domain store |
| Mining result service | Validate normalized results, create candidates, route origin, publish observers | Current: `components/asic/asic_result_handler.c`, `main/tasks/asic_result_task.c` |
| Ports | Work source, ASIC work/event, share sink, and event observer interfaces | Target portable interfaces wired by composition |

## Authority and dependency direction

| Fact or state | Owner | Consumers | Forbidden duplicate |
|---|---|---|---|
| Neutral block work/header | Mining work object | ASIC facade and validation | Protocol object pointer retained by ASIC |
| Work source generation/origin | Work source + pipeline provenance | Share sink/result service | Current global protocol/fallback inference |
| Work purpose | Pipeline work envelope | ASIC/diagnostic observers | Global self-test mode deciding result meaning |
| Hardware slot/packet | ASIC backend | ASIC backend only | Job store exposing raw slot as universal identity |
| Pipeline work handle/generation | Provenance store | ASIC events and result service | Slot-only stale alias after reuse |
| Candidate/accounting event | Mining result service | Share sink and independent observers | One result task calling feature implementations directly |

## Target contracts

- `mining_template_t` contains only header/work facts. Protocol routing moves to
  an opaque, generation-scoped `mining_origin_t` owned by the source adapter.
- `mining_work_t` combines template, purpose, source generation, origin handle,
  and clean-work epoch with explicit clone/free functions.
- `asic_work_t` is the facade submission form; `asic_result_t` and
  `asic_event_t` are normalized backend output.
- `share_candidate_t` contains validated nonce/header/difficulty plus opaque
  origin, not a V1/V2 enum and protocol-owned string pointers.
- Observers receive immutable events and cannot block or control the source
  session. Delivery/backpressure/failure behavior is explicit.

## Current implementation boundary

- **Conforming direction:** Owned template cloning, generation-bearing handles,
  normalized events, capability-driven work cadence, and common validation exist.
- **Violations:** `mining.h` is owned by Stratum while ASIC requires it; templates
  retain protocol enums/strings; work queue payloads are untyped; the work task
  casts V1/V2 objects; the result task calls protocol/self-test/scoreboard/
  monitoring directly; Bitmain slot and generic provenance are co-located;
  self-test parses and enqueues a Stratum notification.
- **Target seam:** Introduce `components/mining` contracts/store first, adapt
  existing Stratum/ASIC APIs, then move work/result services and observers.

## Required and forbidden behavior

- No protocol source may publish work until all untrusted input is validated and
  owned; no ASIC packet is sent until provenance storage succeeds.
- A source close/clean epoch invalidates work before new source work is accepted.
- Observer failure cannot change share validity or block safety-critical result
  handling indefinitely.
- Do not encode protocol choice in ASIC packets/slots or ASIC model in work
  source logic.
- Do not route diagnostic results through a live pool transport.

## Relationships to other feature slices

| Related feature | Relationship |
|---|---|
| [Feature ownership and dependency direction](../../architecture/feature-ownership/SPEC.md) | Defines the portable domain and ports/adapters rules. |
| [ASIC driver contract](../../asic/driver-contract/SPEC.md) | Consumes ASIC work and publishes normalized events. |
| [Stratum session safety](../stratum-session-safety/SPEC.md) | V1/V2 adapters validate and publish work; share sinks encode candidates. |
| [Stratum client lifecycle](../stratum-client-lifecycle/SPEC.md) | Owns source session generation, activation, failover, and close. |
| [Factory self-test lifecycle](../../manufacturing/factory-self-test/SPEC.md) | Publishes diagnostic work and observes purpose-tagged results. |
| [Best-share scoreboard](../../observability/share-scoreboard/SPEC.md) | Observes immutable validated-share events. |

## Verification approach

- Build a standard-C host suite using fake work sources, ASIC port, share sink,
  and observers; retain protocol/ASIC component tests and add QEMU/HIL by risk.
- **Exact revision:** `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f`
- **Evidence classes:** source/test-surface reconciliation only.

## Constraint provenance

- **Current repository evidence:** Existing neutral types and tests establish a
  viable seam; current ownership/location and direct fan-in calls contradict it.
- **Historical review evidence:** Merged capability-driven, shared difficulty,
  work simplification, and result-routing directions are retained in Stratum and
  Bitmain specs. Unresolved unified-`mining_notify` suggestions are not adopted.
- **Disposition:** Target pipeline is implementing; existing helpers are partial
  evidence and migration inputs, not the final module boundary.
