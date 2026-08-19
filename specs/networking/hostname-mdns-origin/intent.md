# Hostname, mDNS, and Origin safety — intent

## Problem

One hostname currently participates in persistent configuration, DHCP, mDNS
conflict handling, redirects, discovery, swarm links, and HTTP Origin checks.
Conflating those roles can cause surprise NVS writes, unstable identity, event
loop stalls, or browser-driven cross-site requests to a miner.

## Why it matters

Operators need a stable configured name, discovery needs a conflict-free runtime
name, and HTTP state changes need a meaningful same-device trust check.

## Stakeholders

- **Operator** — controls persistent identity explicitly.
- **Networking/mDNS** — advertises a valid runtime identity asynchronously.
- **HTTP server** — rejects untrusted browser Origins.
- **AxeOS swarm** — consumes cached advertised identity with an IP fallback.

## Desired outcome

The configured hostname is an RFC 1123 label within the DNS/mDNS length budget.
Conflict suffixes are runtime-only, event callbacks never block on probing, and
Origin-bearing requests are not authorized merely for being private or `.local`.

## Primary flow

1. Firmware validates and persists an operator-supplied hostname.
2. A worker initializes mDNS and derives a conflict-free runtime name.
3. Cached configured/runtime/full names are exposed to API consumers, while
   HTTP authorization compares browser Origin to allowed device identities.

## Alternate and failure flows

- If allocation, task creation, probing, or mDNS setup fails, mining/networking
  continue with an observable discovery failure and the configured name intact.

## Non-goals

- mDNS conflict resolution does not silently rewrite operator configuration.
- Private-address membership alone is not browser-origin authorization.
