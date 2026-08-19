# {{FEATURE_NAME}} — acceptance

Each criterion must be independently verifiable. Check an item only after
recording current evidence.

## Functional behavior

- [ ] **{{SPEC_ID}}-AC-01:** Given {{PRECONDITION}}, when
  {{ACTION_OR_EVENT}}, then {{OBSERVABLE_RESULT}}.
- [ ] **{{SPEC_ID}}-AC-02:** Invalid or unsupported input produces
  {{DEFINED_FAILURE_BEHAVIOR}}.

## Interfaces and compatibility

- [ ] **{{SPEC_ID}}-AC-03:** {{PUBLIC_API_PROTOCOL_CONFIGURATION_OR_HARDWARE_INTERFACE}}
  remains compatible with {{SUPPORTED_CONSUMERS_OR_DEVICES}}.
- [ ] **{{SPEC_ID}}-AC-04:** Any intentional incompatibility has an explicit
  migration, rollout, or recovery path.
- [ ] **{{SPEC_ID}}-AC-05:** Every applicable firmware, protocol/schema,
  generated-client, service, mock, UI, persistence, and migration surface
  agrees on authority, type, units, bounds, defaults, and failure behavior.

## Quality attributes

- [ ] **{{SPEC_ID}}-AC-06:** Applicable security, privacy, hardware safety,
  failure, recovery, and resource requirements are verified.
- [ ] **{{SPEC_ID}}-AC-07:** Applicable timing, memory, flash/image size,
  power, storage, or throughput requirements are met.
- [ ] **{{SPEC_ID}}-AC-08:** Invalid types, encodings, widths, lengths, counts,
  allocation/task failures, and reconnect/reset boundaries have defined and
  focused regression evidence where relevant.

## Verification evidence

| Evidence class | Exact command, case, revision, target, result, and date |
|---|---|
| Focused unit or regression | `{{TEST_COMMAND_OR_CASE}}` — {{RESULT_REVISION_AND_DATE}} |
| AxeOS generation, test, or build | {{COMMAND_RESULT_OR_NOT_APPLICABLE}} |
| Firmware build or QEMU/simulation | {{COMMAND_RESULT_OR_NOT_APPLICABLE}} |
| Authorized device, mining, soak, or HIL | {{TARGET_RESULT_OR_NOT_RUN_WITH_REASON}} |

## Acceptance rule

Work is acceptable only when all affected criteria have current evidence,
related specs are reconciled, and unverified criteria or environment limits are
reported explicitly.
