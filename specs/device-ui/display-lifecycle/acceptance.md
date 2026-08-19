# On-device display lifecycle — acceptance

## Functional behavior

- [x] **UI-DISPLAY-AC-01:** Screen objects and resolution-specific layout are
  selected during `screen_start`, not on every update callback.
- [x] **UI-DISPLAY-AC-02:** Display timeout `-1` means always on, `0` means off
  except pre-carousel/button wake, and positive values apply inactivity sleep
  to normal carousel operation.
- [x] **UI-DISPLAY-AC-03:** Showing pre-carousel/important state and handling a
  button press trigger LVGL activity where required.
- [x] **UI-DISPLAY-AC-04:** Fast display updates are separated from at least
  one-second slower uptime/data work.
- [x] **UI-DISPLAY-AC-05:** The timeout domain is based on arithmetic/storage
  capacity rather than an arbitrary 60-minute product cap.
- [ ] **UI-DISPLAY-AC-06:** Focused tests cover each timeout mode, button wake,
  pre-carousel exemption, identify/block activity, timer cadence, overflow
  boundaries, and representative resolutions.

## Verification evidence

| Evidence class | Exact command, case, revision, target, result, and date |
|---|---|
| Source/schema reconciliation | Inspected `main/screen.c`, NVS descriptor, OpenAPI, and AxeOS display settings at `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f` — AC-01 through AC-05 present, 2026-08-10 |
| Focused regression | Not run; documentation-only reconciliation. AC-06 remains unchecked. |
| Firmware build/simulation | Not run; no implementation changed. |
| Display HIL | Not run; no physical display operation was authorized. |

## Acceptance rule

Changed display timing or hardware layout requires focused automated evidence
and authorized representative-display validation where source tests cannot prove
brightness, orientation, wake, or physical readability.
