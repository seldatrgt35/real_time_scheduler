# ADR-0008: Cortex Assembly/C Boundary

**Status:** Accepted for Version 1

## Decision

Vector handlers, special-register access, context transfer, barriers, and
exception return are pure preprocessed `.S`. Snapshot/startup handoffs,
validation, frame construction, and portable scheduler calls are ordinary C.
Naked C and inline-assembly context handlers are not used.

## Consequences

AAPCS governs bridge calls on aligned MSP. Handler instruction sequences remain
stable and auditable in disassembly while scheduler policy stays testable in C.
