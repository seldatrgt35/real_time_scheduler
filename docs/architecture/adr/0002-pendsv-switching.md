# ADR-0002: PendSV Ordinary Context Switching

**Status:** Accepted for Version 1

## Decision

PendSV is the only ordinary context-switch mechanism and runs at the lowest
implemented configurable priority. It consumes an immutable scheduler snapshot,
saves/restores R4–R11 and PSP, completes the same snapshot, then exception-
returns. It never selects tasks or mutates queues directly.

## Consequences

Coalesced requests require one PendSV notification. A cancelled request may
enter PendSV and harmlessly find no snapshot.
