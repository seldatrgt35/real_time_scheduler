# ADR-0006: Cortex-M4F Initial Frame

**Status:** Accepted for Version 1

## Decision

The initial full-descending stack frame is 64 bytes: R4–R11 followed by the
hardware-compatible basic frame R0, R1, R2, R3, R12, LR, PC, xPSR. R0 carries
the argument; LR is the private trap; PC is entry; xPSR is `0x01000000`; other
registers are zero.

## Consequences

SVC and PendSV use one restore layout. The 64-byte size preserves the public
16-byte alignment contract.
