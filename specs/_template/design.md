# {{FEATURE_NAME}} — design

Describe durable responsibilities and constraints, not a line-by-line source
walkthrough.

## Components and responsibilities

| Component | Responsibility | Implementation pointer |
|---|---|---|
| `{{COMPONENT}}` | {{RESPONSIBILITY}} | `{{MODULE_PATH_SYMBOL_OR_DOCUMENT}}` |

## Authority and dependency direction

Name one owner for each fact or state value. Do not leave authority implicit.

| Fact, state, or identifier | Authoritative owner | Consumers | Forbidden duplicate or upward dependency |
|---|---|---|---|
| {{HARDWARE_FACT_RUNTIME_STATE_OR_PUBLIC_ID}} | `{{OWNING_COMPONENT_SCHEMA_OR_STORE}}` | `{{CONSUMERS}}` | {{SHADOW_STATE_UI_REDERIVATION_MAGIC_STRING_OR_NONE}} |

## Interfaces and contracts

### Firmware, board, or ASIC interface

- {{ESP_IDF_DRIVER_BOARD_CONFIG_OR_NONE}}

### Configuration and persistent state

- {{KCONFIG_NVS_FILE_OR_DEFAULT_OR_NONE}}

### HTTP, WebSocket, or external protocols

- {{OPENAPI_STRATUM_OR_OTHER_PROTOCOL_OR_NONE}}

### AxeOS client and generated API

- {{ANGULAR_COMPONENT_SERVICE_OR_NONE}}

### Files, artifacts, build, flashing, and OTA

- {{ARTIFACT_FORMAT_BUILD_OUTPUT_OR_RECOVERY_CONTRACT_OR_NONE}}

## Cross-layer change matrix

Mark every row applicable or not applicable and describe the required change.

| Surface | Applicability and required reconciliation |
|---|---|
| Firmware authority and validation | {{CHANGE_OR_NOT_APPLICABLE_WITH_REASON}} |
| OpenAPI, WebSocket, Stratum, or other protocol | {{CHANGE_OR_NOT_APPLICABLE_WITH_REASON}} |
| Generated AxeOS client | {{CHANGE_OR_NOT_APPLICABLE_WITH_REASON}} |
| AxeOS model, service, and UI | {{CHANGE_OR_NOT_APPLICABLE_WITH_REASON}} |
| Mock and development data | {{CHANGE_OR_NOT_APPLICABLE_WITH_REASON}} |
| Configuration, NVS, defaults, and migration | {{CHANGE_OR_NOT_APPLICABLE_WITH_REASON}} |
| Tests and operational evidence | {{CHANGE_OR_NOT_APPLICABLE_WITH_REASON}} |

## Contract constraints

### Required invariants

- {{REQUIRED_BEHAVIOR}}

### Forbidden behavior

- {{BEHAVIOR_THAT_MUST_NEVER_OCCUR}}

## Control and data flow

1. {{INPUT_OR_TRIGGER}}
2. {{PROCESSING_OR_STATE_TRANSITION}}
3. {{OUTPUT_OR_SIDE_EFFECT}}

## Failure and recovery

- {{FAILURE_MODE}} → {{DETECTION_CONTAINMENT_RECOVERY_AND_OBSERVABLE_RESULT}}

## Input, ownership, and lifecycle boundaries

- **Untrusted inputs and bounds:** {{TYPE_ENCODING_WIDTH_LENGTH_COUNT_AND_LIMIT_VALIDATION}}
- **Resource ownership:** {{ALLOCATION_BUFFER_CALLBACK_TASK_ARGUMENT_AND_CLEANUP_OWNER}}
- **Concurrency and blocking:** {{TASK_EVENT_LOOP_LOCK_QUEUE_TIMEOUT_OR_NOT_APPLICABLE}}
- **Session/reset boundary:** {{RECONNECT_CLOSE_RESTART_HANDOFF_OR_NOT_APPLICABLE}}

## Compatibility and migration

- {{VERSIONING_ROLLOUT_DEVICE_REVISION_OR_BACKWARD_COMPATIBILITY}}

## Resource and operational constraints

- {{TIMING_MEMORY_FLASH_IMAGE_SIZE_POWER_NETWORK_OR_DEPLOYMENT_CONSTRAINT}}

## Current implementation boundary

- **Conforming:** {{CURRENT_MODULES_ALREADY_FOLLOWING_THE_TARGET_DIRECTION}}
- **Violations:** {{EXACT_CURRENT_MODULES_AND_CONCRETE_UPWARD_OR_SIBLING_DEPENDENCIES}}
- **Target seam:** {{PORT_FACADE_ADAPTER_AND_COMPOSITION_CHANGE}}

## Relationships to other feature slices

| Related feature | Relationship |
|---|---|
| [{{FEATURE}}](../../{{AREA}}/{{SLUG}}/SPEC.md) | {{DEPENDENCY_OR_INTERACTION}} |

## Verification approach

- {{TEST_ANALYSIS_SIMULATION_MEASUREMENT_OR_HARDWARE_STRATEGY}}
- **Exact revision:** {{REVIEWED_COMMIT_OR_NOT_YET_FIXED}}
- **Evidence classes:** {{UNIT_FRONTEND_BUILD_QEMU_SIMULATION_DEVICE_MINING_SOAK_HIL_AS_APPLICABLE}}

## Constraint provenance

- **Current repository evidence:** {{CODE_SCHEMA_TEST_OR_RUNTIME_EVIDENCE}}
- **Historical review evidence:** {{PR_REVIEWER_MERGE_AND_THREAD_DISPOSITION_OR_NONE}}
- **Disposition:** {{ACCEPTED_CORROBORATED_CONTRADICTED_OR_OPEN_CANDIDATE}}
