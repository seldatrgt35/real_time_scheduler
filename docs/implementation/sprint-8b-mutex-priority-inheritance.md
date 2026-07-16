# Sprint 8B — Mutexes and Priority Inheritance

## Public and static object model

`rts_mutex_init`, `rts_mutex_lock`, and `rts_mutex_unlock` operate on a
caller-owned, zero-initialized `rts_mutex_t` with static lifetime. The typed
public C11 layout contains an opaque owner, the same wait-storage component as
semaphores, one ownership link, self identity, and signature. It has no byte
overlay, private TCB layout, architecture state, destroy operation, recursion
count, or ISR API. Initialized mutexes retain a stable address and cannot be
copied, moved, or reinitialized.

Version 1 mutexes are non-recursive. Only the owner unlocks. Self-lock and
non-owner unlock return `INVALID_STATE`; zero-timeout contention returns
`TIMEOUT`. Finite lock accepts `1..RTS_DELAY_MAX`, and `RTS_WAIT_FOREVER` is the
only infinite value.

## Base and effective priority

The private TCB keeps immutable `base_priority` and uses the existing
`priority` field as effective priority. Task creation sets both from the public
descriptor. Every ready-queue index, bitmap selection, waiter comparison, and
time-slice group therefore uses effective priority without a parallel policy
path. `saved_stack_pointer` remains the first TCB member and its static offset
assertion remains zero. The TCB also holds an owned-mutex head, tail, and count;
no packing is used.

For the 32-bit Cortex-M ABI the reviewed layout is 104 bytes with assertions
enabled and 100 bytes without the validation word. Sprint 8 adds 32 bytes per
private TCB relative to the assertion-enabled pre-synchronization layout. The
target's three-slot application pool is 320 bytes including pool counters, and
the separate idle TCB has the same 104-byte layout. These costs are
deterministic; public semaphore and mutex objects remain caller-owned.

Priority change is scheduler-owned. A ready-linked READY or RUNNING task is
removed through the ready API, changed, and inserted at the new FIFO tail. A
blocked object waiter is removed/reinserted through the wait owner so ordering
never becomes stale. No module outside ready/wait owners mutates their links.

## Ownership and direct handoff

Only `mutex.c` changes `mutex->owner` or ownership links. Each mutex is either
unowned/unlinked with no waiters, or linked exactly once in its owner's bounded
list. `RTS_MAX_MUTEXES_PER_TASK` defaults to `RTS_MAX_TASKS` and bounds
acquisition and restoration traversal.

Uncontended lock links the mutex to current in O(1). Blocking lock removes the
current ready membership, installs MUTEX wait metadata, inserts priority/FIFO
wait membership, optionally inserts delay membership, changes RUNNING to
BLOCKED, propagates inheritance, and publishes the established blocked-outgoing
plan.

Unlock with waiters performs direct ownership handoff: the highest effective
priority/oldest waiter leaves both wait structures, becomes the new owner,
receives `ACQUIRED`, becomes READY, and is inserted at its effective-priority
tail. The mutex is never transiently ownerless. The old and new owner priorities
are recomputed before one final scheduler decision.

## Inheritance, propagation, and restoration

For each task:

```text
effective = max(base,
                highest effective waiter on every owned mutex)
```

When a waiter enters, the owner is recomputed. If that owner is BLOCKED on a
mutex, its changed effective priority reorders its wait membership and the walk
continues to the next owner. The iterative walk has no recursion, records
visited tasks, and is bounded by `RTS_MAX_TASKS`. A repeated task is a fatal
corruption signal. Public lock additionally walks the prospective owner chain
before mutation and rejects a multi-task cycle with `INVALID_STATE`; obvious
self-lock is rejected directly.

Restoration uses the same recomputation after unlock, handoff, and waiter
timeout. It never blindly assigns base priority: remaining owned mutexes may
retain a lower inherited level. Priority decrease relocates ready membership
and may cause one planned preemption. Equal-effective tasks follow the chosen
new-FIFO-tail rule and normal yield/slicing thereafter.

## Timeout and arbitration

Finite mutex waiters simultaneously own one wait node and one delay node.
Timeout dispatch removes both, writes `TIMEOUT`, clears waited identity, makes
the task READY, and recomputes the owner chain. Unlock-before-timeout cancels
delay membership during handoff; timeout-before-unlock removes wait membership,
so unlock selects another waiter or clears ownership. PRIMASK serialization
ensures exactly one result, owner, and ready insertion.

Mutex APIs are task-context-only. Semaphore ISR give remains the sole public
ISR synchronization operation and never participates in inheritance.

## Complexity

- uncontended lock: O(1);
- uncontended unlock: O(owned mutexes) restoration plus bounded selection;
- waiter insertion/reprioritization: O(`RTS_MAX_TASKS`);
- direct handoff/removal: O(1);
- restoration: O(`RTS_MAX_MUTEXES_PER_TASK`) per chain level;
- propagation: at most `RTS_MAX_TASKS` levels;
- no heap, task-pool scan, recursion, or pointer ordering.

## Verification and target evidence

Host tests cover initialization, stable ownership, context/misuse, try-lock,
basic and medium-inversion inheritance, priority/FIFO handoff, two owned mutexes
with 5→4→base restoration, wrapped finite timeout restoration, direct handoff,
transitive H6→M3→L1 inheritance, wait reprioritization, bounded cycle detection,
and 1,000 two-mutex lock/unlock cycles. The Sprint 8A 2,000-event semaphore
stress and all delay/time-slice/scheduler regressions remain passing under
assertion-enabled, assertion-disabled, and slicing-disabled builds.

The S32K148 smoke image adds a low-priority mutex owner, medium READY task, and
high waiter. Debug fields record low acquisition/release and high handoff. The
same image retains semaphore ISR give and finite timeout observations. Strict
Cortex-M4 Thumb soft-float compilation passes; physical evidence is pending.
