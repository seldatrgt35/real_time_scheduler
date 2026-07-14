# ADR-0003: SVC First-Task Launch

**Status:** Accepted for Version 1

## Decision

First-task launch uses a dedicated startup handoff and SVC. The portable start
transaction establishes current/state and RUNNING lifecycle before SVC. SVC
restores the selected initialized frame without creating or completing a fake
outgoing switch.

## Consequences

Failures are recoverable only before SVC. Because PRIMASK blocks SVC activation,
startup clears a previously-clear mask immediately before `svc`; the handler
remasks on entry and scheduler-aware interrupt sources remain disabled across
that window. Successful launch does not return; unexpected return is fatal.
