# On-device display lifecycle — intent

## Problem

Displays vary in resolution and layout. Re-selecting layouts or refreshing
expensive data at every fast UI tick wastes resources, while a global sleep rule
can blank setup, recovery, or active interaction screens.

## Why it matters

Operators need readable device-specific screens, responsive buttons, and
predictable sleep behavior without burdening mining tasks.

## Stakeholders

- **Operator** — sees setup/fault/interaction screens and can wake the display.
- **Display/device configuration** — owns hardware type, resolution, and layout.
- **System state** — supplies cached data without display-owned duplication.

## Desired outcome

Layout and carousel are chosen at display startup. A fast presentation tick and
slower data refresh remain separate, and inactivity sleep applies only to normal
carousel screens with explicit activity reset.

## Primary flow

1. `screen_start` reads the selected display/resolution and creates its screens.
2. Timers update presentation and slower data at bounded cadences.
3. Carousel inactivity turns the display off; a button or important system event
   records activity and wakes it as specified.

## Alternate and failure flows

- Setup, recovery, self-test, identify, or other pre-carousel states remain
  visible according to their safety/interaction contract.

## Non-goals

- Different resolutions need not share an identical carousel or layout.
- External smart-display firmware owns its own layout/rotation/sleep lifecycle.
