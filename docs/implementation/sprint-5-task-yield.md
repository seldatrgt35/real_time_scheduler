# Sprint 5.4 — Public Task Yield

## Public contract

`rts_task_yield()` is a task-context operation valid only while the kernel is
`RUNNING`. Calls from interrupt context return `RTS_STATUS_INVALID_CONTEXT`.
Calls in `RESET` or `INITIALIZED` return `RTS_STATUS_INVALID_STATE`. A valid
call returns `RTS_STATUS_OK`, including when no switch is needed.

Yield is strictly equal-priority. It never makes a lower-priority task eligible
to run while the current task remains ready. Priority zero is not special-cased:
the idle task has no application peer and therefore yields as a successful
no-op.

## Transaction and ownership

The operation first validates ISR context, then enters the port critical
section. Under that protection it rechecks lifecycle and verifies that the
current task is `RUNNING`, runnable, ready-linked, the FIFO front at its
priority, and that no switch snapshot is active.

With no peer, the operation exits immediately without changing queue order,
task state, switch-plan state, or port-request state. With a peer it uses the
ready-queue rotation API, reselects through the scheduler selector, and asks
the Sprint 5.3 planner to prepare or coalesce the switch. Only the planner's
fresh-plan result authorizes a port notification. The critical section is
exited before that notification.

The ready queue exclusively owns intrusive-link mutation. Yield does not
directly inspect list links and does not change `current_task`, task states,
wait metadata, delay membership, stack data, or pool metadata. In accordance
with the accepted Version 1 public baseline, every valid yield reloads the
caller's slice counter.

## State sequence

For `A(RUNNING) -> B(READY) -> C(READY)` at one priority, yield rotates the FIFO
to `B -> C -> A` and prepares `A -> B`. Before snapshot completion, A remains
current and `RUNNING`, while B remains `READY`. Sprint 5.3 completion changes A
to `READY`, B to `RUNNING`, and current to B without relinking the queue.

A second public yield by A before the pending transfer is treated as an
impossible kernel execution state: A is no longer the FIFO front. The invariant
is asserted and no further mutation occurs. Other scheduler planning events may
still coalesce a PENDING plan through the existing Sprint 5.3 planner without a
duplicate port notification. Public yield while a snapshot is ACTIVE is also an
asserted internal-state violation and cannot mutate the queue or snapshot.

## Time slicing

Voluntary yield always rotates when an equal-priority peer exists, independent
of `RTS_ENABLE_TIME_SLICING`. The accepted public baseline requires every
successful yield, including a no-peer yield, to reload the current task's
counter from `RTS_TIME_SLICE_TICKS`; it never changes the incoming task's
counter. Tick accounting remains outside Sprint 5.4.

## Verification

Host tests cover RESET, INITIALIZED, ISR rejection, application and idle no-op,
lower-priority exclusion, three-task FIFO rotation, switch acquisition and
completion, bounded repeated round robin, nested critical restoration, port
notification count, and internal invariant guards. The same implementation is
built with assertions enabled and disabled and with time slicing enabled and
disabled.

Build and run with:

```text
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Actual context transfer, target exception wiring, tick slicing, and first-task
launch remain future port/scheduler work.
