# Sprint 7B — Task Delay, Delayed Wakeup, and Runtime Preemption

## Scope

Sprint 7B implements relative task blocking, ordered expiry, READY restoration,
and higher-priority wakeup preemption. It does not decrement slice counters,
rotate equal-priority queues from ticks, implement timers, synchronization,
absolute sleep, tickless idle, or ISR-facing public APIs.

## Public delay semantics

`rts_task_delay(delay)` is valid in task context while the scheduler is RUNNING
and the non-idle current task is coherently RUNNING at the head of its ready
FIFO. The valid nonzero range is 1 through `0x7fffffff`; larger values return
`RTS_STATUS_INVALID_ARGUMENT` before mutation. ISR calls return
`RTS_STATUS_INVALID_CONTEXT`, and RESET/INITIALIZED calls return
`RTS_STATUS_INVALID_STATE`.

Delay zero delegates directly to `rts_task_yield()`. It therefore has identical
status, FIFO rotation, switch planning, and time-slicing-configuration behavior
and never touches delay metadata or the delay queue.

The absolute deadline is sampled and calculated once inside the kernel critical
section:

```text
wake_tick = current_tick + delay        (uint32_t modulo arithmetic)
```

No signed conversion, epoch, or raw unsigned deadline ordering is used.

## Blocking transaction

After lifecycle and current-task preconditions are rechecked under the critical
token, `task_delay.c` performs this frozen sequence:

1. assign the absolute wake tick;
2. remove the RUNNING current task through the ready-queue API;
3. set wait reason to DELAY;
4. transition RUNNING to BLOCKED;
5. insert through the ordered delay-queue API;
6. select the highest READY task (idle guarantees a result);
7. prepare or coalesce the blocked-current to selected switch plan;
8. restore the exact critical token;
9. notify the port only for a newly published plan.

The final pre-transfer invariant is BLOCKED+DELAY, ready-unlinked,
delay-linked, and `current_task` still referencing the physically executing
outgoing task. Queue modules alone mutate their intrusive nodes. Valid private
queue operations cannot fail recoverably; failed postconditions are fatal
kernel-corruption assertions rather than public partial-success paths.

Calling public delay from the private idle task is an internal invariant
violation: assertion builds report it and the API returns
`RTS_STATUS_INVALID_STATE` without inserting idle into the delay queue.

## Blocked-outgoing context switch

The switch planner now recognizes exactly two outgoing forms:

- RUNNING and runnable/ready-linked for yield or preemption;
- current BLOCKED+DELAY, ready-unlinked, delay-linked, valid application TCB for
  a just-published blocking switch.

Incoming remains a distinct runnable READY task. Snapshot acquisition preserves
the same immutable pointer/generation contract. Completion changes RUNNING
outgoing to READY, but leaves BLOCKED outgoing BLOCKED and preserves its wait
metadata. Incoming always changes READY to RUNNING and becomes current. The
Cortex bridge accepts and saves the blocked outgoing context without reinserting
it or changing queue membership.

ACTIVE snapshots remain immutable. Tick activity that discovers a newer
candidate sets `reselection_required`. After the active Cortex transfer
completes, the bridge invokes the portable deferred-reselection helper and
requests a later PendSV only when it publishes a fresh higher-priority plan.

## Tick expiry and wakeup

The tick entry advances once by the bounded elapsed value and repeatedly asks
only for the due delay-queue head. It does not scan the task pool or the full
queue. For every due task it validates application-pool identity, allocation,
saved context, priority, stack metadata, BLOCKED+DELAY state, exclusive delay
membership, and absence from ready queues. It then:

1. removes through the delay-queue API;
2. clears wait reason and neutralizes wake tick to zero;
3. resets `slice_remaining` to `RTS_TIME_SLICE_TICKS`;
4. changes BLOCKED to READY;
5. inserts at the tail through the ready-queue API.

Equal deadlines retain delay FIFO order, and their ready-tail insertion uses
that same extraction order. All tasks due at the final tick are awakened before
one selection decision. Multi-tick advancement does not iterate per tick.

If an unusually short delay expires before its already-requested PendSV runs,
the task is still the physically executing `current_task`. Wakeup then restores
that task directly to RUNNING while relinking it ready, and selection cancels
the stale pending blocking plan when it remains highest. The already-pended
hardware exception consequently observes no plan and is a harmless no-op. This
prevents a READY physical current or an invalid BLOCKED snapshot.

## Preemption policy

After wake-all:

- a higher-priority selected task preempts a normal RUNNING current task;
- an application wake preempts RUNNING idle because priority zero is lower;
- equal-priority wakeup is appended but does not preempt;
- lower-priority wakeup does not preempt;
- a BLOCKED current awaiting its PendSV always retains/coalesces the required
  outgoing plan;
- a PENDING plan may retarget to a newly awakened higher task without another
  hardware notification;
- an ACTIVE plan is never changed and records deferred reselection instead.

At most one plan and one port notification decision are produced after all
expirations. Actual transfer remains PendSV-owned.

## ISR boundary and critical sections

`rts_kernel_tick_advance()` returns a Boolean notification decision. The strong
S32K148 SysTick handler acknowledges CTRL/COUNTFLAG, calls the portable entry
with one, calls `rts_port_request_context_switch()` once only when requested,
and exits. Target code does not inspect tasks, queues, priorities, or planner
state and never executes PendSV inline.

Task blocking is protected by the existing PRIMASK critical token. SysTick is a
maskable kernel-aware interrupt above PendSV and cannot observe the intermediate
ready removal/state/delay insertion sequence. Wake-all runs coherently in the
ISR; PendSV mutation masks eligible interrupts under the accepted Cortex
contract.

## Wraparound and elapsed bounds

Deadlines and time use modulo-2^32 arithmetic with the shared strict half-range
helpers. The exact-half-range relation remains ambiguous and invalid for queue
ordering. A deadline such as `0xfffffff0 + 0x20 = 0x00000010` blocks and expires
correctly. One elapsed call remains bounded to `0x7fffffff`; all deadlines
crossed by its final tick are extracted once.

## Verification

Focused integration tests cover public validation, exact delay-zero delegation,
blocking membership and metadata, blocked-outgoing completion, wrapped deadline,
pre-deadline and exact-deadline behavior, full-quantum wakeup, higher-priority
preemption, equal/lower no-preemption, equal-wake FIFO order, pending-plan
coalescing without duplicate notification, all-application-blocked idle fallback,
idle preemption, and later lower-priority wakeup.

Existing tick, delay-queue, switch-plan, Cortex bridge, scheduler-start, and
target SysTick tests remain part of the regression set. Portable units compile
with strict C11 warnings. Cortex task-delay, tick, target ISR, and switch bridge
compile for Cortex-M4/Thumb/soft-float/no-FPU. Physical hardware execution was
not available in this workspace.

The S32K148 smoke application now runs Task A at priority 3 and Task B at
priority 2. A validates context/register preservation, then delays for ten
ticks. B increments while A is blocked; A's expiry prepares deferred PendSV and
preempts B. The debugger record continues to expose both counters, sampled tick,
PSP/MSP evidence, stack guards, and failure flags.

## Remaining Sprint 7C work

Sprint 7C owns tick-driven slice decrement, quantum expiry, equal-priority FIFO
rotation, combined wakeup/slice selection, final integration acceptance, and
physical timing evidence. Sprint 7B deliberately resets quantum on wake but
never decrements it.
