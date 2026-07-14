# Sprint 5.3 — Switch Plan Preparation and Coalescing

**Status:** Implemented; ready for yield integration  
**Scope:** Portable runtime switch planning, immutable snapshot, and completion

## Ownership and boundary

The scheduler owns `current_task`, task states, candidate validation, the single switch plan, snapshot identity, and completion. The ready queue remains the sole owner of links, FIFO ordering, and bitmap state. The selected port records/executes a transfer request but never chooses a task or writes task state.

Sprint 5.3 performs no register save/restore, saved-SP mutation, queue mutation, time advance, task blocking/wakeup, FIFO rotation, first-task launch, PendSV, SVC, SysTick, or host task execution.

## Private state machine

```text
INACTIVE
  from=NULL, to=NULL, pending=false, active=false

      prepare(current != selected)
                 |
                 v
PENDING
  from=current, to=latest selected, pending=true, active=false
  repeated selection coalesces by replacing only to

      acquire immutable snapshot
                 |
                 v
ACTIVE
  from/to/generation frozen, pending=false, active=true
  new events set reselection_required only

      complete matching snapshot
                 |
                 v
INACTIVE
  current=snapshot.to; task states committed
  reselection_required is retained until next preparation
```

There is exactly one plan and at most one active snapshot. No A→B→C chain is stored.

## Preparation and coalescing

`rts_scheduler_prepare_switch(next)` is a pure planner mutation and returns `true` only when an inactive plan becomes newly PENDING and the architecture needs notification.

Preconditions are RUNNING lifecycle, nonnull valid current task, and a valid runnable selected task. The selected task must be READY unless it is the current RUNNING task.

- `next == current` with no plan is an idempotent no-switch result.
- `next != current` creates `{from=current,to=next}` and advances generation.
- Repeating the same pending incoming task changes nothing.
- A different latest candidate replaces only `to`, retains the actual current `from`, and advances generation.
- Returning to current before acquisition cancels the pending plan, clears both pointers, advances generation, and changes no task or queue state.

`rts_scheduler_request_switch_if_needed(next)` is the narrow orchestration boundary. It calls `rts_port_request_context_switch()` only when preparation returns `true`. Coalescing, repetition, and cancellation do not notify again.

If cancellation occurs after a hardware request was recorded but before exception consumption, the hardware request may still arrive. Snapshot acquisition then finds no pending plan and returns false, producing a harmless no-op. The portable planner does not attempt architecture-specific unpending.

## Snapshot acquisition

`rts_scheduler_switch_acquire(snapshot)` copies `from`, `to`, and the 32-bit generation into a caller-owned `rts_switch_snapshot_t`, then changes PENDING to ACTIVE. It returns false when no plan is pending. Null output, duplicate acquisition, lifecycle misuse, identical tasks, current mismatch, illegal states, or non-runnable members are assertion violations.

The future port consumes only this local snapshot; it never reads mutable planner pointers during transfer. While ACTIVE, preparation validates the latest candidate but cannot alter `from`, `to`, or generation and cannot cancel. It sets `reselection_required=true` and returns no new notification.

This chooses the recommended deferred-reselection model. The active A→B operation completes exactly as acquired. Events that occur meanwhile are reconsidered against new current B afterward.

## Completion

`rts_scheduler_switch_complete(snapshot)` accepts only the exact ACTIVE generation and pointer pair. It verifies:

- RUNNING lifecycle;
- active, non-pending plan;
- generation and pointer identity;
- `current_task == snapshot.from`;
- outgoing RUNNING and incoming READY;
- both tasks remain runnable and ready-linked.

Completion changes outgoing RUNNING→READY, incoming READY→RUNNING, and then assigns `current_task=incoming`. It clears plan pointers and ACTIVE state while preserving `reselection_required`. It does not select, notify the port, or mutate either queue node, so FIFO order and bitmap state are unchanged.

Duplicate, stale, cancelled, mismatched, wrong-current, or wrong-state completion asserts and performs no mutation.

After an event during ACTIVE, the caller observes `rts_scheduler_switch_reselection_required()`, reruns highest-ready selection after completion, and calls preparation. If another switch is needed, a new generation and architecture notification are created.

## Generation semantics

Generation is a private `uint32_t` advanced whenever a pending plan is created, materially retargeted, or cancelled. Acquisition freezes its value. Equality with the one active plan protects completion against stale and mismatched snapshots.

Modulo wrap is permitted. Only one snapshot may be legitimately outstanding, and it blocks plan-generation changes until completion; copies retained after completion are no longer valid outstanding snapshots. Therefore equality remains sufficient under the bounded-outstanding contract without 64-bit arithmetic.

## Invariants

| State | Required invariant |
|---|---|
| INACTIVE | pending=false, active=false, from/to null; current may remain RUNNING |
| PENDING | pending=true, active=false, from=current RUNNING, to distinct READY task |
| ACTIVE | pending=false, active=true; snapshot and plan pointers/generation identical and immutable; current remains from |
| After completion | current=snapshot.to RUNNING; snapshot.from READY; both remain ready-linked; plan inactive/null |

During all states, the planner changes no priority, wait metadata, saved SP, stack bound, intrusive node, queue count, bitmap, or lifecycle.

## Critical-section contract

Preparation, orchestration, acquisition, and completion require the caller to hold the approved kernel critical/masking contract. The functions do not acquire internally and expose no public critical API. This permits a future scheduler event or PendSV wrapper to protect selection plus plan mutation with one architecture token and avoids hidden nesting.

The host tests are single-core deterministic fixtures; host critical primitives and LIFO tests already validate the token mechanism. The future PendSV wrapper must acquire and complete snapshots while its eligible-interrupt mask prevents concurrent planner mutation.

## Host-port request recording

The host port records a monotonically checked request count and a pending-notification flag without executing a task. Private test controls inspect both and mark a request consumed when emulating exception entry. Reset clears all request state. These controls do not inspect or mutate scheduler state and do not leak through `kernel/port.h` or public headers.

## Tests and integration scenarios

Focused tests cover:

- A→B plan creation, no-plan A→A, idempotent A→B, and A→B→latest-C coalescing;
- fixed outgoing A, unchanged current/states, and exactly one port notification;
- pre-acquisition cancellation back to A without another notification;
- null/current/lifecycle misuse and BLOCKED, DORMANT, RESERVED, or non-ready candidates;
- acquire with no plan, correct immutable snapshot, and duplicate acquisition;
- simple A→B completion and unchanged membership/FIFO links;
- stale generation, mismatched pointers, wrong current, incoming-not-READY, and duplicate completion rejection;
- ACTIVE A→B receiving latest A/C events without cancellation or snapshot mutation;
- completion to B, reselection of C, and fresh B→C plan with a second notification.

The integration fixtures bootstrap the kernel, create real application TCBs/stacks through public creation, adopt a coherent private RUNNING current, and exercise the planner without context execution.

Normal build flow is CMake/CTest with strict C11 and warnings-as-errors. The current environment additionally executes assertion-enabled and assertion-disabled freestanding WebAssembly tests and compiles the production source as a Cortex-M4 translation unit.

## Remaining dependencies

- `rts_start()` must use the dedicated first-task path rather than creating a null-outgoing switch plan.
- Public yield/delay and tick logic must perform their queue/state work, reselect, then call the planner wrapper under one critical section.
- The Cortex-M4F PendSV path must consume immutable snapshots, save/restore SPs, and call completion without choosing tasks.
- Host and target request-pending mechanics must treat cancelled/no-plan exceptions as no-ops.
