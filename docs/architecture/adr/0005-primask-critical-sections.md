# ADR-0005: PRIMASK Critical Sections

**Status:** Accepted for Sprint 6

## Decision

Critical entry snapshots PRIMASK and disables configurable interrupts. Exit
restores the exact prior value through the opaque portable token contract.

## Consequences

The first hardware implementation is simple and deterministic, but all
configurable interrupt latency includes kernel critical-section duration.
The token API permits a future separately approved BASEPRI migration.
