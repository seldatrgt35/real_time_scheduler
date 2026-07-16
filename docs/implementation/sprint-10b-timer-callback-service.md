# Sprint 10B Deferred Timer Callback Service

## Scope and final architecture

Sprint 10B completes software timers without executing application code in an
interrupt or kernel critical section:

```text
target tick ISR
    -> portable elapsed-tick transaction
    -> dedicated active-timer queue
    -> fixed callback-work FIFO
    -> private timer-service task
    -> typed application callback in Thread mode
```

The timer manager owns private timer objects and their active nodes. The
callback ring owns copied work metadata. The scheduler owns the service TCB,
READY/BLOCKED transitions, selection, and switch planning. The service task
alone invokes callbacks. `RTS_MAX_TASKS` remains application capacity; idle and
timer service are separate typed private TCBs.

## Configuration and bootstrap

Every image selects:

- `RTS_MAX_TIMERS`;
- `RTS_TIMER_SERVICE_PRIORITY`, in `1..RTS_PRIORITY_COUNT-1`;
- `RTS_TIMER_SERVICE_STACK_SIZE_BYTES`, aligned and port-validated;
- `RTS_TIMER_CALLBACK_QUEUE_CAPACITY`, at least `RTS_MAX_TIMERS`.

`rts_init()` resets the manager and ring, constructs the service TCB and stack
through the normal port stack contract, and leaves it BLOCKED with private wait
reason `RTS_WAIT_TIMER_SERVICE`. It has no public handle and consumes no pool
slot. Bootstrap failure restores canonical RESET. `rts_start()` requires the
service task to be blocked and the callback queue empty. Timers may be
registered and armed in INITIALIZED; their deadlines begin at tick zero and
advance only after the target tick starts.

The configured service priority is ordinary fixed priority. Higher-priority
application tasks outrank it; lower-priority tasks and idle are preempted when
callback work wakes it. Equal-priority behavior follows normal FIFO/time-slice
rules. A high service priority reduces callback latency but increases
interference, so callback WCET belongs in application schedulability analysis.

## Orthogonal timer state machines

Model A periodic reload means a timer can be active for its next deadline while
a previous callback is pending or running. One field cannot express both facts
without contradiction, so the private object uses two orthogonal machines.

Lifecycle:

```text
UNINITIALIZED --init--> STOPPED
STOPPED --start/restart--> ACTIVE
ACTIVE --stop--> STOPPED
ACTIVE --one-shot expiry--> STOPPED
ACTIVE --periodic expiry/reload--> ACTIVE
```

Callback work:

```text
IDLE --expiry/enqueue--> PENDING
PENDING --service dequeue--> RUNNING
RUNNING --callback return--> IDLE
PENDING --stop/restart invalidation--> IDLE
```

`start` accepts only STOPPED+IDLE; it never silently restarts. `restart` is the
explicit deadline reset and invalidates queued work. `stop` succeeds when it
cancels an active deadline or pending/running callback epoch; stopping an
already quiescent timer returns `RTS_STATUS_INVALID_STATE`, preserving the
established public style. A running callback is never cancelled retroactively.

## Callback queue and generation

The kernel-owned callback queue is a fixed FIFO ring. Each 16-byte ARM work item
contains a typed timer pointer, consumed expiration tick, generation, and
expiration sequence. Enqueue/dequeue are O(1). Equal deadlines retain active
queue FIFO order, so ring order is deterministic without address tie-breaking.
The active intrusive node is never used by the callback queue.

At most one work item per timer may be pending. Stop/restart increments the
unsigned generation and performs a bounded O(queue capacity) removal of queued
work for that timer. The service also checks generation immediately before
invocation, providing a defensive stale-work barrier. A full callback queue is
`RTS_FATAL_TIMER_CALLBACK_QUEUE_OVERFLOW`; work is never overwritten or
silently dropped. Requiring queue capacity at least timer capacity makes
overflow unreachable under valid one-pending invariants. Generation wrap is
safe under the bounded assumption that no work survives 2^32 invalidations.

## One-shot and periodic semantics

A one-shot expiry removes the active entry, changes lifecycle to STOPPED, and
enqueues exactly one callback. After invocation it remains STOPPED unless the
callback explicitly restarts it.

Periodic timers use deadline-relative Model A:

```text
next = previous_deadline + N * period
```

At expiry, the next deadline is calculated and reinserted before callback work
is published. Callback latency therefore does not create period drift. For an
elapsed-tick jump, division selects the first deadline strictly after `now` in
bounded time. Missed periods are coalesced into one callback and counted.

If another period expires while the timer already has PENDING or RUNNING work,
the active deadline still advances, but no duplicate work is added. Overrun and
missed-expiration counters increment. This bounds backlog to one callback per
timer and prevents catch-up storms.

## Service execution and callback rules

The service drains the FIFO before blocking again. Queue-empty observation and
the BLOCKED transition share one critical transaction, closing the lost-wakeup
window. The first enqueue wakes a blocked service task; later enqueues do not
wake it repeatedly. Tick processing performs delayed-task wakeups, timer
expiry/service wakeup, time-slice accounting, one final selection, and at most
one fresh switch notification.

Before invoking a callback the service dequeues and marks RUNNING under the
critical section, copies the typed callback and argument, exits the critical
section, and then calls it. Callback begin/end trace events bracket ordinary
Thread-mode execution. Callbacks are serialized and never run from tick ISR,
PendSV, SVC, or while list invariants are transient.

Callbacks may:

- start, stop, or restart timers, including themselves;
- give semaphores;
- take an immediately available semaphore;
- lock an immediately available mutex and unlock owned mutexes;
- yield.

Version 1 deliberately rejects operations that would block the single timer
service: positive task delay, unavailable semaphore take, and contended mutex
lock return `RTS_STATUS_INVALID_CONTEXT`. A callback may not call ISR APIs as
Thread APIs. This is a reviewed deviation from the optional blocking-callback
model and prevents one callback from indefinitely delaying all later timers.
Callbacks must remain short; the kernel creates no task per timer.

Self-stop removes a periodic future deadline, invalidates queued work, and lets
the current callback finish. Self-restart replaces the Model A deadline with
`current_tick + period`; generation arbitration prevents automatic handling
from overwriting it. A callback may safely control another timer because no
timer-manager critical section is held during invocation.

## Diagnostics and validation

Runtime counters cover callbacks executed, stale work, coalesced periods,
overruns, maximum queue depth, and overflow attempts in addition to Sprint 10A
counters. Per-timer counters cover callback, stale, missed, and overrun events.
Trace adds callback begin/end, stale, and overrun events.

Validation checks active ordering and node ownership, ring bounds and indices,
typed pool ownership, work generation, one queued item per PENDING timer,
callback flags, service task identity/membership, queue-to-service availability,
and both private TCBs outside application capacity.

## Complexity and deterministic RAM

| Operation | Bound |
| --- | --- |
| Active insertion | O(`RTS_MAX_TIMERS`) |
| Earliest active lookup | O(1) |
| Expire k due timers | O(k), with bounded arithmetic per timer |
| Callback enqueue/dequeue | O(1) |
| Stop/restart stale purge | O(`RTS_TIMER_CALLBACK_QUEUE_CAPACITY`) |
| Service drain | O(number of queued callbacks plus callback WCET) |
| Full timer invariant scan | O(`RTS_MAX_TIMERS` x queue capacity) |

For the diagnostics-enabled S32K148 configuration measured with Clang ARM32:

- private timer: 88 bytes;
- callback work item: 16 bytes;
- eight-entry callback ring including indices: 144 bytes;
- eight-timer manager including active queue and callback ring: 868 bytes;
- timer-service TCB: 128 bytes;
- configured timer-service stack: 768 bytes.

The release timer object is 52 bytes and the corresponding eight-timer manager
is 580 bytes. All storage is static; there is no allocation, recursion, or
unbounded catch-up traversal.

## Verification

Focused host tests cover bootstrap, capacity, one-shot exact-once execution,
deadline and FIFO order, periodic Model A reload, coalescing, overrun,
wraparound, stale invalidation, self-stop, self-restart, another-timer control,
semaphore give, blocking rejection, queue capacity, preemption/switch completion,
and a fixed-seed 50,000-event stress sequence with invariants after every event.
Debug, release, and no-time-slicing profiles are exercised.

The S32K148 smoke image adds periodic callback evidence, one-shot semaphore
signalling, callback PSP/IPSR capture, service-task identity, and application
stop/restart. Physical execution remains unclaimed until board evidence is
captured.
