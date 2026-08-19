# Factory self-test lifecycle — intent

## Problem

Factory/manual self-test coordinates display messages, a reset semaphore, fan,
power, ASIC work, temperature, hashrate measurements, NVS flags, and restart.
Retaining stack pointers or continuing after failed initialization makes a
diagnostic path itself unsafe.

## Why it matters

A failed unit must remain safely powered and display a trustworthy, stable
result until an explicit restart/reset path is available.

## Stakeholders

- **Manufacturing/operator** — receives a stable pass/fail reason and recovery.
- **Self-test task** — owns its buffers, semaphore, measurements, and cleanup.
- **Power/thermal/ASIC layers** — provide safe operations and authoritative data.

## Desired outcome

Self-test is a portable policy/state machine. It consumes an immutable board
qualification profile and injected power, thermal, ASIC-diagnostic, metrics,
status, persistence, and platform ports. All retained messages outlive their
consumers, initialization failures terminate the flow, and every terminal path
forces a known safe hardware state.

## Primary flow

1. Initialize owned synchronization and measurement state, then mark test active.
2. Run applicable bounded fan, voltage, temperature, domain, and hashrate checks
   through injected ports while publishing copied status text. Diagnostic work
   enters the neutral mining work pipeline with `SELF_TEST` purpose; it does not
   impersonate a pool session.
3. Stop measurements, power/reset ASIC safely, persist the result/flag policy,
   and wait for or perform the documented restart path.

## Alternate and failure flows

- Any failed prerequisite publishes one owned error message, stops further
  unsafe steps, cleans resources, and enters the terminal failure path.

## Non-goals

- Self-test does not invent separate fan, voltage, sensor, or ASIC-init rules.
- Self-test policy does not parse Stratum, write a Stratum queue, or set
  protocol flags to manufacture diagnostic work.
- Self-test policy does not include concrete regulator, board, ASIC-backend,
  NVS, GPIO, or ESP-IDF APIs, and does not mutate production device settings to
  create a diagnostic mode.
- Source review is not a substitute for authorized factory-board validation.
