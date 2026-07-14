# Sprint 4.5 — Task Creation Transaction and Ready Registration

**Status:** Implemented; ready for review  
**Scope:** Version 1 startup-only `rts_task_create()` transaction

## Transaction boundary

`rts_task_create()` integrates the approved descriptor validator, static pool, private TCB initializer, selected-port stack constructor, and fixed-priority ready set. It does not select or execute a task, update `current_task`, request a switch, process time, or implement scheduler start.

The kernel owns one static `rts_kernel_state_t`, returned through a private accessor. This gives the future `rts_init()` and scheduler implementation the same state object without exposing a writable public global.

## Ordered operation

```text
validate out-handle and clear it
reject ISR context
validate descriptor and INITIALIZED lifecycle
enter port critical section
recheck INITIALIZED lifecycle
reserve FREE pool slot
initialize RESERVED TCB as DORMANT
construct architecture/host initial stack frame
store returned saved SP
insert at ready FIFO tail
transition DORMANT -> READY
commit RESERVED -> ALLOCATED
set assertion validity magic
publish exact TCB pointer
exit critical section
```

Publication is the final externally observable creation step. Before it, the output handle remains null. The validity magic is set only after ready registration and pool commit. The committed count excludes every unfinished transaction.

No scheduler selection or preemption check occurs because Version 1 creation is legal only in `INITIALIZED`, before scheduler start.

## Validation and status mapping

- Null output pointer returns `RTS_STATUS_INVALID_ARGUMENT`.
- A nonnull output is cleared before any later check.
- ISR context returns `RTS_STATUS_INVALID_CONTEXT` and asserts when enabled.
- Descriptor, lifecycle, priority, and stack errors retain Sprint 4.2 statuses.
- No free slot returns `RTS_STATUS_CAPACITY_EXHAUSTED` without mutation.
- A recoverable selected-port construction failure is returned and rolls back the slot.

Lifecycle is checked before and again inside the critical section. Runtime creation in `RUNNING` and creation before initialization both return `RTS_STATUS_INVALID_STATE`.

## Rollback

The common rollback path removes ready membership if it was established, resets all private task fields and both nodes to their neutral representation, then changes the still-RESERVED slot back to FREE. Pool count is unchanged, the hint returns to the failed slot, no magic remains, and no handle is published.

The port is allowed to have modified caller-owned stack bytes before reporting failure; those bytes never become a runnable or published context. All kernel-owned publication state is nevertheless restored.

After successful pool commit there is no ordinary fallible operation. Internal ready-set or commit corruption remains an assertion/fatal invariant; defensive guards prevent handle publication if an assertion hook unexpectedly returns in a host test.

## Ready registration

The ready queue exclusively mutates `ready_node` and associates it with the exact TCB. Creation inserts at the tail of the task's fixed-priority FIFO. The task remains delay-unlinked with wait reason NONE. Only after confirmed ready membership does orchestration change state from DORMANT to READY.

The idle task is not involved, application priority zero remains rejected, and the application-pool count excludes idle.

## Host support

The host port now supplies deterministic ISR-context reporting and prior-state critical sections with assertion-checked LIFO tokens. Host-only controls reset port test state, select simulated ISR context, inspect critical depth, and inject one stack-construction failure. These controls are private to `ports/host/port_internal.h` and are absent from the portable/public contract.

## Tests

Focused tests cover successful direct-handle publication, complete TCB state, decoded initial host frame, ready membership, highest-ready lookup, equal-priority FIFO order, exact capacity and exhaustion, null output, RESET/RUNNING rejection, invalid priority, ISR rejection, critical-section balance, and injected stack failure with complete pool/TCB/ready rollback.

Both assertion-enabled and assertion-disabled configurations use the same functional transaction behavior.

## Remaining limitations

- `rts_init()` must initialize the shared pool, ready/delay structures, idle task, selected port, and lifecycle before application creation.
- Target critical sections and Cortex-M4F stack construction remain required for hardware integration.
- Task deletion and runtime task creation are absent by Version 1 design.
- Public yield/delay, scheduler start, selection, ticks, and context switching remain unimplemented.
