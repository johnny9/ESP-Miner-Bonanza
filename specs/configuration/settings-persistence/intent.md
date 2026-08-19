# Settings validation and persistence — intent

## Problem

Settings can change fan, thermal, voltage, frequency, networking, and protocol
behavior. Partial writes, implicit conversions, or unplanned NVS key/type
changes can make a request unsafe or strand users across firmware versions.

## Why it matters

A settings request must either be wholly valid or make no change. Stored values
must remain interpretable across supported upgrade and downgrade paths.

## Stakeholders

- **Operator** — receives precise rejection instead of a partial unsafe update.
- **Firmware** — owns validation, persistence types, defaults, and migrations.
- **AxeOS and API clients** — consume the same types, bounds, and enum domains.

## Desired outcome

Every public setting has one authoritative descriptor and any narrower
field-specific rule. The complete request is checked before queuing writes, and
storage migrations are explicit and reversible where compatibility requires it.

## Primary flow

1. A client sends a settings PATCH containing one or more fields.
2. Firmware validates every supplied type, range, enum, and hardware constraint.
3. Only a fully valid request is applied; invalid input returns HTTP 400 with no
   requested field persisted.

## Alternate and failure flows

- Allocation, parse, lookup, or persistence failures are observable and do not
  turn into successful partial configuration.
- A renamed or retyped key is read through an explicit migration path; a legacy
  mirror is retained only when downgrade compatibility is intentional.

## Non-goals

- Browser-only presentation preferences are not device settings and do not
  belong in NVS.
- This spec does not define board-specific safe voltage or frequency values;
  device configuration owns those facts.
