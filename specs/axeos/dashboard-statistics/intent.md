# Dashboard statistics selection — intent

## Problem

Operators choose which telemetry appears on two chart axes. Treating that
presentation choice as device configuration wastes flash writes and couples
firmware to browser labels.

## Why it matters

The device should expose stable telemetry identifiers and bounded history while
each browser owns its presentation preferences.

## Stakeholders

- **Operator** — keeps per-browser Y1/Y2 choices without changing the miner.
- **Firmware** — returns requested columns in a self-describing order.
- **AxeOS** — maps stable IDs to labels, units, and axes.

## Desired outcome

Chart selection lives in browser storage, requests use stable JSON/URL-safe
keys, and the response labels define each data-row column order.

## Primary flow

1. AxeOS loads Y1/Y2 stable keys from local storage or defaults.
2. The service requests the required unique columns from system statistics.
3. AxeOS uses response labels to map ordered values and renders display labels.

## Alternate and failure flows

- Unknown or unavailable series is omitted or shown as unavailable without an
  NVS write or positional guess.

## Non-goals

- This spec does not define telemetry sampling internals or persist chart
  presentation on the device.
