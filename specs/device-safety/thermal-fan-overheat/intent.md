# Thermal, fan, and overheat control — intent

## Problem

Boards differ in sensor topology, fan floors, voltage control, and ASIC reset
requirements. Spreading model checks across generic tasks or allowing a pause
state to mask overheat can damage hardware or create inconsistent recovery.

## Why it matters

Thermal control is a hardware-safety boundary. Invalid readings, failed fan
writes, overheat, or restart failures must fail safe and remain diagnosable.

## Stakeholders

- **Operator/device** — receives cooling and safe shutdown before convenience.
- **Board/thermal layer** — owns sensors, offsets, fan capabilities, and limits.
- **Power-management task** — owns mining stop/start and safety priority.
- **ASIC driver** — owns chip-specific frequency/reset/serial operations.

## Desired outcome

Generic controllers consume an immutable safety profile and thermal, fan,
power, reset, and ASIC-lifecycle ports; they never include a concrete board,
bridge, regulator, ESP driver, or ASIC model. Fan output is bounded, invalid
temperature cannot drive PID, and overheat forces the shared safe stop/restart
path even when mining is otherwise paused.

## Primary flow

1. Thermal/power layers publish validated readings and board limits.
2. Fan control selects safety, pause, automatic PID, or manual behavior in a
   defined priority and clamps output to the supported range.
3. Power management detects overheat, stops through the shared lifecycle, proves
   cooling, and resumes only through validated initialization.

## Alternate and failure flows

- Missing telemetry, fan write failure, power validation failure, or ASIC init
  failure holds or returns the device to a safe non-mining state.

## Non-goals

- Generic tasks do not encode per-ASIC sensor offsets or transition algorithms.
- Generic safety policy does not branch on `CONFIG_BZM_*`, call the Bonanza
  bridge, or include a concrete regulator/sensor/ESP driver.
- UI settings do not override a hardware floor or active safety state.
