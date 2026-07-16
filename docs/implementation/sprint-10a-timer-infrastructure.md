# Sprint 10A Software Timer Infrastructure

> Sprint 10B supersedes the callback-free runtime boundary in this document.
> Pool ownership, active-queue ordering, and public handles remain valid; final
> execution semantics are defined in
> `sprint-10b-timer-callback-service.md`.

## Scope

Sprint 10A adds the static software-timer subsystem and tick-side expiration
detection. It deliberately does not execute callbacks, wake tasks, request
context switches, or automatically re-arm periodic timers. Those behaviors
belong to Sprint 10B.

```text
Application timer API
        |
        v
Timer manager and fixed pool
        |
        v
Dedicated ordered timer queue <--- scheduler tick
        |
        v
EXPIRED state only (no callback execution)
```

The task delay queue remains unchanged and contains only delayed tasks. The
timer queue contains only private `struct rts_timer` objects. The scheduler
selection and switch-planning modules do not inspect timer objects.

## Public contract

`include/rts/rts_timer.h` adds:

- opaque incomplete-pointer `rts_timer_handle_t`;
- `RTS_TIMER_ONE_SHOT` and `RTS_TIMER_PERIODIC`;
- callback type and a descriptor containing period, callback, argument, mode;
- `rts_timer_init`, `rts_timer_start`, `rts_timer_stop`,
  `rts_timer_restart`, and `rts_timer_is_running`.

Registration is startup-only in `INITIALIZED`. Control calls are valid in
`INITIALIZED` and RUNNING task context. ISR calls are rejected; Sprint 10A has
no ISR timer-control API. Period must be `1..RTS_DELAY_MAX`, callback must be
non-null, and mode must be one of the two defined values. The callback and
argument are stored but the callback is never invoked in this sprint.

## Static ownership and pool

The kernel owns exactly `RTS_MAX_TIMERS` typed timer objects and a separate
timer manager. Registration performs a bounded first-free search using a
next-free hint. Slots follow:

```text
FREE -> RESERVED -> ALLOCATED
```

Reservation exists so registration remains transactional. Version 1 has no
timer deletion, so an allocated slot never returns to free. Handles point
directly to actual pool elements, remain stable, and are validated by bounded
pool identity comparison before dereference. No heap, byte overlay, node
allocation, or unrelated-type cast is used.

## Timer state machine

```text
UNINITIALIZED --rts_timer_init--> STOPPED
STOPPED -------start/restart----> RUNNING
RUNNING -------stop-------------> STOPPED
RUNNING -------restart----------> RUNNING (new deadline)
RUNNING -------tick due---------> EXPIRED
EXPIRED -------start/restart----> RUNNING
```

Starting an already RUNNING timer, stopping a non-running timer, invalid
handles, and invalid lifecycle/context return deterministic errors without
partial queue mutation. Restart replaces an active arm atomically.

For a periodic expiration, `last_expiration_tick` records the consumed
deadline and `expiration_tick` is advanced by exactly one period as preparation
for Sprint 10B. The timer remains EXPIRED and unlinked. This avoids silently
executing a periodic policy before callback context and missed-period behavior
are approved.

## Ordered queue

`timer_queue.c` exclusively owns mutation of each timer's `queue_node`. It
uses the generic intrusive-list mechanism but has its own list, contracts, and
validation; it does not reuse the ready or delay queue.

Insertion walks from the earliest deadline using the approved modular
half-range comparison. Equal deadlines retain registration/arm FIFO order.
The head is the earliest timer, so expiry lookup is O(1). Removal is O(1), and
insertion is bounded O(`RTS_MAX_TIMERS`). Queue validation checks endpoints,
owner links, node/object reciprocity, RUNNING membership, count bounds, and
deadline ordering.

## Tick integration

After advancing the global tick and processing delayed tasks,
`rts_kernel_tick_advance()` drains all due timer heads. Each due timer is
removed and marked EXPIRED. Expiration updates bounded diagnostics and emits a
numeric trace event. It does not call the stored callback and does not affect
the tick function's preemption decision. Task wakeup and time slicing continue
to be the only tick-side causes of a switch plan in Sprint 10A.

## Diagnostics and validation

Optional per-timer counters record starts, stops, restarts, and expirations.
Kernel counters add initialization/start/stop/restart/expiration totals. Trace
adds five timer events without strings or callbacks. Full invariant validation
checks pool counts, slot/state validity, callback/period/mode fields, queue
membership, and ordering. It compiles to a constant success result when
invariant checks are disabled.

## Complexity

| Operation | Bound |
| --- | --- |
| Manager initialization | O(`RTS_MAX_TIMERS`) |
| Registration/handle validation | bounded O(`RTS_MAX_TIMERS`) |
| Start/restart insertion | bounded O(`RTS_MAX_TIMERS`) |
| Stop/removal | O(1) after bounded handle validation |
| Earliest expiration lookup | O(1) |
| Tick expiration processing | O(k), where k is due timers |
| Full manager validation | O(`RTS_MAX_TIMERS`) |

There is no recursion or unbounded traversal.

## Memory

Measured Clang layouts are:

| Object | ARM32 release | ARM32 diagnostics | Host64 diagnostics |
| --- | ---: | ---: | ---: |
| private timer | 44 B | 64 B | 96 B |
| eight-slot manager including queue metadata | 372 B | 532 B | 808 B |
| public timer descriptor | 16 B | 16 B | 32 B |

Diagnostics therefore add 20 bytes per ARM timer plus five 32-bit kernel
counters. Capacity is entirely compile-time selected through
`RTS_MAX_TIMERS`; no timer RAM is allocated dynamically.

## Verification

Focused tests cover descriptor validation, registration, capacity exhaustion,
start/duplicate start, cancellation/double stop, restart from every supported
state, ordered and equal-deadline FIFO insertion, tick expiration, periodic
next-deadline preparation, wraparound, ISR rejection, lifecycle rejection,
queue corruption detection, and callback non-execution.

A fixed-seed 20,000-operation host stress sequence mixes tick advancement,
start, stop, restart, and queries while validating the timer manager and whole
kernel after every event. It runs with diagnostics enabled, release diagnostics
disabled, and time slicing disabled. Strict Cortex-M4F soft-float compilation
verifies the target integration without changing SVC/PendSV assembly.

## Sprint 10B boundary

Sprint 10B must define callback execution context, serialization, callback
latency, periodic missed-deadline policy, callback-driven stop/restart rules,
and whether an internal timer service task is required. It may consume EXPIRED
timers and their prepared periodic deadline. It must not execute application
callbacks in the tick ISR or while the kernel critical section is held unless
a separately reviewed contract explicitly changes this boundary.
