# ADR-0007: Saved-SP Offset Contract

**Status:** Accepted for Version 1

## Decision

Assembly accesses TCB saved SP through a shared symbolic offset. Cortex-only C
statically verifies that symbol against `offsetof(struct rts_task,
saved_stack_pointer)`. No C accessor call or duplicated raw assembly number is
used.

## Consequences

Access remains two direct memory operations while layout drift becomes a build
failure. A generated-offset step may later replace the checked shared header.
