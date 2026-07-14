# Sprint 4.3 — Private TCB Initialization

**Status:** Implemented; ready for review  
**Scope:** Initialization of one already-reserved application TCB

## Boundary and preconditions

`rts_task_object_initialize()` receives the application pool, the exact TCB candidate, an already validated public descriptor, and a lifecycle snapshot. It requires a nonnull candidate that is found by bounded pointer-equality scan in that pool, `RESERVED` slot ownership, `INITIALIZED` lifecycle, and canonical-unlinked ready and delay nodes. It asserts contract violations and returns defensively without mutation if the assertion hook returns.

The bounded membership scan compares pointers only for equality. It performs no relational comparison, subtraction, representation conversion, allocation, reservation, or pool metadata mutation.

Descriptor validation is not repeated. Constant-time assertions recheck only the essential handoff assumptions needed to avoid invalid memory arithmetic or an invalid task object: nonnull entry/stack, nonzero and non-overflowing stack extent, public start alignment, and application-priority range. Port minimum/granularity queries remain owned by Sprint 4.2 validation and will be checked independently again by the Sprint 4.4 stack constructor.

## Initialized representation

| Field | Sprint 4.3 value |
|---|---|
| `saved_stack_pointer` | Null; explicitly invalid until Sprint 4.4 succeeds |
| `stack_low` | Descriptor stack start |
| `stack_high` | Exclusive `stack_low + stack_size_bytes` bound |
| `entry`, `argument`, `priority` | Copied from the validated descriptor |
| `ready_node`, `delay_node` | Canonical-unlinked through `rts_list_node_initialize()` |
| `wait.reason`, `wait.wake_tick` | `RTS_WAIT_NONE`, zero |
| `slice_remaining` | Configured `RTS_TIME_SLICE_TICKS`, regardless of slicing enablement |
| `state` | `RTS_TASK_STATE_DORMANT` |
| `slot_state` | Preserved as `RTS_TASK_SLOT_RESERVED` |
| `validation_magic` | Zero when assertions are enabled |

`DORMANT` is the approved unpublished state and does not falsely claim ready eligibility before queue registration. The unconditional slice field retains the approved stable TCB layout; assigning the configured quantum follows the Sprint 2 initial-value contract without selecting or rotating a task.

Magic zero deliberately distinguishes an initialized reserved object from a committed valid handle. Later transaction orchestration owns ready insertion, transition to READY, pool commit, valid magic publication, and handle publication.

## Ownership and rollback

The application retains stack ownership for the task lifetime. Sprint 4.3 records bounds but writes no stack byte and constructs no architecture frame. The pool allocation count and next-free hint are unchanged. Because the slot remains RESERVED and unpublished, later failure can still roll it back.

The initializer calls only the intrusive-node initializer. It calls neither the ready queue nor delay queue, and it performs no scheduler, port, exception, or context-switch action.

## Layout verification

The existing compile-time assertion keeps `saved_stack_pointer` at offset zero. Focused tests also check the offset in both assertion configurations. Assertion-only metadata remains after all assembly-visible and release fields, so it cannot move the saved-stack-pointer field.

## Tests

Focused tests verify every initialized field, exclusive stack bounds, nullable argument copying, DORMANT/NONE state, configured slice value, canonical nodes, null saved SP, zero magic, preserved RESERVED ownership, unchanged pool count/hint, and that the entry function is not called. Contract tests cover null inputs, non-pool TCBs, wrong lifecycle, non-reserved ownership, malformed node state, and an invalid descriptor handoff.

## Remaining limitations

- Architecture frame construction is supplied by the Sprint 4.4 port contract. The later creation transaction still owns storing the returned saved SP in the TCB.
- Sprint 4.5 now owns ready insertion, READY transition, pool commit, valid magic, and handle publication.
- Raw reserved nodes must already have a valid canonical-unlinked representation. Production pool storage is static-zero initialized; rollback/reset orchestration must restore that representation before reuse.
- Version 1 has no task deletion or reinitialization of committed TCBs.
