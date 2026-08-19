# Hostname, mDNS, and Origin safety — acceptance

## Functional behavior

- [ ] **NET-IDENTITY-AC-01:** Settings accept only an RFC 1123 hostname label,
  reject leading/trailing hyphens and invalid characters, and enforce the full
  advertised-name length budget including `.local` and any conflict suffix.
- [x] **NET-IDENTITY-AC-02:** mDNS initialization is spawned outside the Wi-Fi
  event callback, task-creation failure is checked, and probe allocations are
  checked before use.
- [ ] **NET-IDENTITY-AC-03:** A conflict-derived mDNS name updates only runtime
  cached identity. Current initialization and update paths write it to NVS.
- [x] **NET-IDENTITY-AC-04:** System info serves cached runtime identity instead
  of performing an mDNS lookup for every request.

## Security and compatibility

- [ ] **NET-IDENTITY-AC-05:** An Origin-bearing request is accepted only when
  its parsed scheme/host/optional development port matches an explicitly
  allowed identity for this device. Current code broadly accepts any private
  IPv4 Origin and any `.local` or bare hostname.
- [x] **NET-IDENTITY-AC-06:** API consumers can select runtime full hostname,
  configured hostname, then IP/connection address as compatibility fallbacks.
- [ ] **NET-IDENTITY-AC-07:** Focused tests cover RFC 1123 boundaries, suffix
  length, conflicts, allocation/task failure, no implicit NVS write, IPv4/IPv6,
  ports, missing/malformed/foreign Origin, and redirects.

## Verification evidence

| Evidence class | Exact command, case, revision, target, result, and date |
|---|---|
| Source/schema reconciliation | Inspected `components/connect/connect.c`, HTTP Origin/hostname code, system info, OpenAPI, and swarm links at `1c0d299aeae7e4a42e1cc0e4de2ecb405120387f` — AC-02, AC-04, AC-06 present; AC-01, AC-03, AC-05 gaps found, 2026-08-10 |
| Focused regression | Not run; documentation-only reconciliation. AC-07 remains unchecked. |
| Firmware/AxeOS build | Not run; no implementation changed. |
| Device/network test | Not run; no device or private network operation was authorized. |

## Acceptance rule

Identity changes require validation, persistence, runtime discovery, API,
redirect, Origin, and swarm-link evidence; security gaps cannot be waived by UI
validation.
