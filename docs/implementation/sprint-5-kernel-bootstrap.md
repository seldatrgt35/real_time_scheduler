# Sprint 5.1 — Kernel Bootstrap and `rts_init()`

**Status:** Implemented; ready for review  
**Scope:** One-shot Version 1 kernel initialization through INITIALIZED

## Bootstrap transaction

`rts_init()` accepts only non-ISR RESET state. It returns `ALREADY_INITIALIZED` after success and `ALREADY_STARTED` if lifecycle is RUNNING. ISR invocation returns `INVALID_CONTEXT` and asserts when configured.

The successful transaction is:

```text
reject ISR context
validate RESET lifecycle
enter selected-port critical section
recheck RESET lifecycle
restore the static kernel aggregate to canonical RESET
initialize application pool ownership
initialize ready set
initialize delay queue
initialize selected architecture port
initialize separate idle TCB as DORMANT
construct idle initial stack through the port
store idle saved SP
insert idle at ready priority zero
transition idle DORMANT -> READY
activate idle assertion metadata
publish private idle pointer
publish lifecycle INITIALIZED last
restore prior critical state
```

No current task is selected, no switch plan is prepared, no context-switch request occurs, and no task executes. `current_task` remains null. Tick and switch-plan fields remain their canonical zero values.

## Initialization ownership

- The scheduler bootstrap owns the static kernel aggregate, lifecycle publication, private idle object, and idle entry.
- The pool initializes application slot ownership and metadata. Idle remains outside that pool.
- Ready and delay modules initialize and mutate their own structures.
- The selected port initializes architecture state and constructs the idle frame.
- The task-creation transaction is not reused for idle because it rejects priority zero, consumes application capacity, and publishes a public handle.

The idle TCB uses the ordinary private type but has its own object and aligned `RTS_IDLE_STACK_SIZE_BYTES` array. Its slot metadata is ALLOCATED for internal object validity, while application-pool committed count remains zero.

## Idle invariant after success

The private idle object is READY, priority zero, ready-linked as the sole priority-zero member, delay-unlinked, wait NONE with zero wake tick, initialized with the configured slice value, and has valid stack bounds, entry, saved SP, and assertion magic. It is the highest-ready result only while no application task exists. It cannot run before a future successful `rts_start()`.

The idle entry is a private non-returning loop. Sprint 5.1 constructs its context but does not transfer execution to it or implement low-power behavior, hooks, statistics, or a public idle API.

## Failure and retry

Recoverable selected-port initialization or idle-stack construction failure maps to `RTS_STATUS_PORT_ERROR`. Before return, the complete kernel aggregate is restored to static-zero RESET and the exact prior critical state is restored. No idle pointer, ready membership, application allocation, current task, switch plan, or INITIALIZED lifecycle survives.

The architecture port has no deinitialize contract. Therefore a port whose general initialization succeeded before later idle-frame failure must support a safe repeated `rts_port_initialize()` while kernel lifecycle remains RESET. The host implementation is idempotent and tests this retry path. This is a required target-port bootstrap contract.

Internal ready-registration corruption follows the fatal invariant path rather than becoming a recoverable initialization condition.

## Host-port support

The host port now implements deterministic `rts_port_initialize()`. Host-private controls can inject one initialization failure and query initialized state. Existing stack-failure injection exercises the later rollback point. These controls remain under `ports/host/port_internal.h` and do not leak into portable or public headers.

## Status precedence

| Condition | Result |
|---|---|
| ISR context | `RTS_STATUS_INVALID_CONTEXT` |
| Lifecycle INITIALIZED | `RTS_STATUS_ALREADY_INITIALIZED` |
| Lifecycle RUNNING | `RTS_STATUS_ALREADY_STARTED` |
| Unexpected lifecycle representation | Assertion; defensive `RTS_STATUS_INVALID_STATE` |
| Port initialization failure | `RTS_STATUS_PORT_ERROR` |
| Idle-stack construction failure | `RTS_STATUS_PORT_ERROR` |
| Successful bootstrap | `RTS_STATUS_OK` |

Lifecycle is checked again inside the critical section before any reset or mutation.

## Complexity and RAM

Bootstrap clears the statically bounded kernel aggregate, initializes `RTS_MAX_TASKS` pool slot ownership entries, initializes `RTS_PRIORITY_COUNT` ready queues and bitmap words, initializes one delay list, and constructs one fixed-size port frame. Worst-case time is O(`sizeof(rts_kernel_state_t) + RTS_MAX_TASKS + RTS_PRIORITY_COUNT`) with all quantities compile-time bounded.

No dynamic RAM is used. Kernel-state RAM already includes the application pool, separate idle TCB, idle stack, ready set, delay queue, lifecycle/current/tick/switch metadata, pool count, and hint.

The entire operation is inside the startup critical section. This bounded cost occurs before any task executes and is not a runtime scheduling-latency path.

## Tests

Focused host tests cover:

- complete successful bootstrap and idle TCB/frame invariants;
- empty application pool and initialized ready/delay structures;
- null current task, zero tick, and empty switch plan;
- repeat initialization and RUNNING status mapping;
- ISR rejection and assertion behavior;
- recoverable port-initialization failure with canonical RESET;
- recoverable idle-stack failure with canonical RESET;
- balanced critical depth on every path;
- successful retry after both failure points;
- application task creation after initialization, with idle retained outside capacity and the application task becoming highest ready.

Both assertion-enabled and assertion-disabled builds execute the same valid bootstrap behavior.

## Remaining limitations and Sprint 5 dependencies

- `rts_start()` and first-task transfer remain unimplemented.
- `current_task` intentionally remains null until scheduler start.
- Runtime selection, switch plans, yield, delay, ticks, and context switching remain absent.
- The Cortex-M4F port must implement idempotent initialization, critical primitives usable during bootstrap, and real idle/application initial frames.
- Target timer and interrupt-vector integration remain outside bootstrap.
