# Sprint 8A — Wait Infrastructure, Semaphores, Timeout, and ISR Give

## Scope and public API

Sprint 8A adds one counting-semaphore implementation whose maximum count of one
also provides binary-semaphore semantics. It adds no mutex ownership, priority
inheritance, recursive behavior, events, messages, allocation, or deletion.

```c
rts_status_t rts_semaphore_init(rts_semaphore_t *, rts_count_t initial,
                                rts_count_t maximum);
rts_status_t rts_semaphore_take(rts_semaphore_t *, rts_tick_t timeout);
rts_status_t rts_semaphore_give(rts_semaphore_t *);
rts_status_t rts_semaphore_give_from_isr(rts_semaphore_t *, bool *notify);
```

`RTS_WAIT_FOREVER` is exactly `UINT32_MAX`. Finite waits use
`0..RTS_DELAY_MAX`; values in the unused open interval are rejected. `TIMEOUT`
covers immediate unavailability and elapsed finite wait. `FULL` reports give
at maximum count.

## Static object representation

The application provides a zero-initialized `rts_semaphore_t` with static
lifetime. The public struct is a deliberate typed C11 ABI containing count,
maximum, opaque task endpoints, waiter count, self identity, and signature. It
contains no TCB layout, architecture state, private list node, byte overlay, or
aliasing trick. Initialization is one-shot. Its address is stable and an
initialized object must never be copied, moved, or modified by application
code. Release validity requires self identity, signature, count bounds, and a
coherent bounded wait chain; it does not rely solely on debug magic.

## Reusable wait-object mechanism

Every private TCB has a dedicated `rts_wait_node_t` in addition to ready and
delay nodes. Only `wait_object.c` mutates it. A semaphore stores only opaque
chain endpoints. Insertion walks at most `RTS_MAX_TASKS` tasks and orders larger
numeric priority first while walking past equal-priority waiters. This produces
strict priority then FIFO without pointer ordering. Removal, highest extraction,
and timeout cancellation are O(1); validation is bounded O(`RTS_MAX_TASKS`).
The compact concrete mechanism is reusable by Sprint 8B without callbacks.

## TCB wait state and memberships

The private wait record contains reason, result, absolute wake tick, waited
object, and finite-timeout marker. Results are `NONE`, `ACQUIRED`, `TIMEOUT`,
and reserved internal `CANCELLED`. A semaphore waiter is:

```text
BLOCKED + SEMAPHORE + result NONE
ready node unlinked
wait node linked to exactly one stable semaphore
delay node linked iff timeout_active
```

A normal delayed task remains `BLOCKED + DELAY`, has no object membership, and
does not use `timeout_active`. A task awakened by handoff or timeout has no wait
or delay membership, is READY (or restored RUNNING in the pre-PendSV race), and
retains its result until the suspended public take consumes it.

## Take and direct handoff

Available take decrements once under the critical section and performs no
scheduling. Zero-timeout failure returns without metadata or queue mutation. A
blocking take revalidates the RUNNING non-idle current, removes it through the
ready owner, initializes metadata, inserts through the wait owner, optionally
inserts through the delay owner, changes it to BLOCKED, selects another task,
and publishes the established blocked-outgoing plan. PendSV remains the only
transfer.

Give selects the highest-priority oldest waiter. It removes object membership,
cancels finite delay membership, writes `ACQUIRED` once, clears wait identity,
restores READY, and inserts at the ready FIFO tail. Count remains unchanged:
availability transfers directly and cannot be stolen. With no waiter, give
increments once or returns `FULL` without mutation.

## Timeout and race arbitration

Finite semaphore waits reuse the ordered absolute delay queue. Tick orchestration
extracts due heads and dispatches by wait reason; the delay queue stays pure
mechanism. Timeout removes both memberships, writes `TIMEOUT` once, clears wait
identity, and makes the task READY. All due work precedes one final selection.

Give and expiry serialize under the same single-core PRIMASK contract.
Give-before-expiry removes the delay node, so later expiry cannot find the task.
Expiry-before-give removes the wait node, so later give increments count or
wakes another waiter. The winner performs the sole result write and READY
insertion. A wake before already-pending blocking PendSV restores the physical
current to RUNNING and cancels/retargets the stale plan. ACTIVE snapshots stay
immutable and use deferred reselection.

## ISR give and preemption

The ISR variant is strict ISR-context-only, nonblocking, and uses the same
critical/direct-handoff helpers without calling the task API. It never calls
PendSV. Its Boolean is the coalesced port-notification decision: the ISR ORs it
with other work and requests PendSV once on exit. Higher-priority and idle wake
publish a plan; equal/lower wake does not preempt solely because of give.
Equal peers join the ready tail and wait for yield or slice expiry.

S32K148 synchronization-aware interrupts must be maskable by PRIMASK, call no
blocking API, and remain above PendSV. BASEPRI is not introduced. SysTick
combines tick and smoke-hook decisions before one request.

## Complexity and ownership

- immediate take/give without waiter: O(1);
- ordered insertion: bounded O(`RTS_MAX_TASKS`);
- handoff and timeout cancellation: O(1);
- final selection: bounded by ready bitmap width;
- tick expiry: O(k) due tasks plus bounded object removals;
- no pool scan, heap, recursion, callbacks, or architecture dependency.

Ready, delay, and wait modules exclusively mutate their nodes. Semaphore owns
count and synchronization metadata. Scheduler owns state/current/plans. Port
and target own context detection and PendSV notification.

## Verification

Freestanding host executables cover initialization, reinitialization,
binary/counting bounds, immediate operations, full/timeout statuses, context
separation, finite/forever blocking, dual membership, blocked completion,
priority-plus-FIFO order `A5, C5, B3`, direct handoff, equal-priority
no-preemption, idle preemption, middle timeout removal, both race winners,
wraparound, result consumption, time slicing, assertions on/off, slicing off,
and 2,000 deterministic semaphore cycles with invariants after every cycle.
Sprint 7 delay and slicing regressions pass with the extended dispatcher.

Portable units compile with strict C11 warnings. Cortex-M4 Thumb soft-float
optimized compilation passes. Static scans find no allocation and ISR give
contains no blocking public call.

The S32K148 smoke application owns a binary semaphore. Its weak target tick
hook override performs ISR give every twenty ticks. High-priority A alternates
forever acquisition and three-tick finite timeout while lower B/C continue time
slicing. Debugger-visible give, acquired, timeout, task, stack, register, and
fault fields provide evidence. Physical execution is pending.

## Sprint 8B integration

Sprint 8B reuses waiter ordering, node ownership, timeout arbitration, direct
wake, and planner integration. Mutex ownership, base/effective priority,
ready/wait reordering, bounded transitive inheritance, and multi-owned
restoration are documented in the Sprint 8B implementation record. Semaphore
counting and ISR semantics remain independent of ownership/inheritance.

## Sprint 8A Architecture Review

The implementation preserves static allocation, strict typed C11 access,
single-core PRIMASK serialization, fixed-priority selection, equal-priority
FIFO, queue ownership, scheduler-owned state, immutable ACTIVE snapshots,
PendSV-only transfer, one-notification coalescing, and portable/target
separation. The approved public extension is the typed static semaphore API,
count/timeout scalar additions, and two minimal statuses. No accepted
architecture contradiction remains.
