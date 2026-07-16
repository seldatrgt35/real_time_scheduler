# Sprint 12 Scheduler Policy Framework

**Status:** Implemented and host-verified  
**Scope:** Compile-time selection of fixed-priority, rate-monotonic, or earliest-deadline-first scheduling.

## Architecture

The portable scheduler owns lifecycle, task states, critical sections, current-task ownership, switch planning, and architecture notification. It no longer owns ready ordering. All ready-set operations cross `scheduler_policy.h`; policy implementations never perform a context switch or mutate scheduler lifecycle.

```text
scheduler lifecycle/state
          |
          v
 scheduler_policy contract
   |          |          |
   FP         RMS        EDF
bitmap/FIFO bitmap/FIFO ordered intrusive list
```

Exactly one of `RTS_POLICY_FIXED_PRIORITY`, `RTS_POLICY_RMS`, and `RTS_POLICY_EDF` is `1`. Public preprocessing rejects missing, non-Boolean, and multiple selections. Dispatch is compiled away; there is no run-time registration table or policy switch.

## Policy contract

The private contract consists only of initialization, insertion, removal, next-task selection, yield, block, unblock/release, effective-priority change notification, elapsed-tick processing, and bounded validation. `scheduler_policy.c` selects one plugin and records release metadata. The scheduler core does not inspect bitmaps, FIFO queues, periods, or deadlines.

The scheduler continues to own task state. A policy owns `ready_node` membership and ordering. Insertion/unblock records `release_tick`, recomputes `absolute_deadline`, and assigns a monotonically ordered release sequence. Blocking removes membership. Context-switch code and the `saved_sp` offset are unchanged.

## Timing metadata

The private TCB contains base and effective priority plus period, relative and absolute deadline, release tick, execution-budget placeholder, and release sequence. `saved_sp` remains the first TCB field and its compile-time offset assertion remains zero.

The startup descriptor supplies `period`, `relative_deadline`, and `execution_budget` because the Version 1 closed task set has no other timing-registration operation. Values are copied into the private TCB. The budget is validated against the deadline where one is required but is not consumed or enforced; admission control remains future work.

- FP accepts optional half-range timing metadata and schedules only by effective priority.
- RMS requires `0 < deadline <= period <= RTS_DELAY_MAX` and `budget <= deadline`.
- EDF requires `0 < deadline <= RTS_DELAY_MAX`, optional half-range period, and `budget <= deadline`.

Application priority remains syntactically valid in every profile because synchronization and effective-priority bookkeeping retain that field. RMS replaces base/effective scheduling ranks during startup registration; EDF does not use priority for ready ordering.

## Fixed-priority plugin

FP is the accepted bitmap plus one FIFO list per priority. Larger numeric effective priority wins. Yield rotates only an equal-priority FIFO; time slicing uses the same rotation. The running task remains linked. This is a mechanism move, not a behavioral change.

Initialization is `O(RTS_PRIORITY_COUNT)`. Insert, remove, and FIFO rotation are `O(1)`. Highest-ready lookup is deterministic and bounded by bitmap width. Full validation is `O(RTS_PRIORITY_COUNT + N)`.

## Rate-monotonic plugin

RMS derives ranks from the closed startup task set: a shorter period receives a larger numeric priority. Equal periods receive one rank and retain FIFO ordering. When a startup task is registered, the bounded pool is rescanned and already registered application tasks are re-ranked safely. Idle remains priority zero and the private timer-service task retains its reserved service priority. After registration, RMS delegates ready membership, selection, yield, blocking, wakeup, priority inheritance, and slicing to the FP mechanism.

Startup rank assignment is `O(N^2)` per insertion with `N <= RTS_MAX_TASKS`; this is outside runtime scheduling. Runtime insert/remove/yield follow FP bounds. No schedulability or utilization admission test is performed.

## Earliest-deadline-first plugin

EDF owns one intrusive ordered ready list. Non-idle tasks are ordered first by wrap-safe absolute deadline, then by release sequence. The release sequence provides FIFO order for equal deadlines. Idle is always the final fallback. Each insertion after a release recomputes `absolute_deadline = release_tick + relative_deadline`; a blocked task therefore receives a new deadline on every wake.

Insertion is bounded `O(N)`. Removal and head selection are `O(1)`. Yield is `O(N)` only when it rotates among equal-deadline peers because the task is reinserted. Validation is bounded `O(N)`. All modular comparisons rely on the approved half-range tick contract.

## Integration

Delay expiry, synchronization wakeup, timer-service wakeup, priority inheritance, yield, time slicing, scheduler start, switch reselection, invariant checking, and tickless-idle eligibility use only the common policy contract. The power subsystem still computes elapsed time and wake sources independently; it asks the policy only whether idle is the selected runnable task. Software-timer expiry wakes the timer service through the same release path.

The S32K148 smoke target selects `FP`, `RMS`, or `EDF` with `RTS_S32K148_POLICY`. All profiles use the same kernel and Cortex-M4F context code; only the configuration include directory changes. Smoke task periods/deadlines intentionally produce A, C, B order under both RMS/EDF and match the existing FP priorities.

## Complexity

| Operation | FP | RMS runtime | EDF |
| --- | --- | --- | --- |
| insert | `O(1)` | `O(1)` after startup ranking | `O(N)` |
| remove | `O(1)` | `O(1)` | `O(1)` |
| pick next | bounded bitmap lookup | bounded bitmap lookup | `O(1)` |
| yield | `O(1)` | `O(1)` | `O(N)` when rotating peers |
| validate all | `O(P + N)` | `O(P + N)` | `O(N)` |

RMS startup rank assignment is bounded `O(N^2)` per registered task.

## Validation and tests

Focused host profiles cover compile-time selection, FP regression, RMS rank assignment and equal-period FIFO behavior, EDF deadline ordering, equal-deadline FIFO/yield, per-wakeup deadline refresh, wraparound, blocking/wakeup, structural validation, and eight-task sets. Existing Sprint 0-11 tests remain the FP regression suite. Strict C11 warning-as-error compilation is applied to all three policy configurations.

Target builds are selected as follows:

```text
-DRTS_BUILD_S32K148_SMOKE=ON -DRTS_S32K148_POLICY=FP
-DRTS_BUILD_S32K148_SMOKE=ON -DRTS_S32K148_POLICY=RMS
-DRTS_BUILD_S32K148_SMOKE=ON -DRTS_S32K148_POLICY=EDF
```

Physical execution remains board/lab work. The deterministic task metadata and selected order are identical across host and target builds.

## Invariants

- One and only one policy is compiled into an image.
- A READY or RUNNING task belongs to exactly one selected-policy ready structure.
- The scheduler never mutates ready links directly.
- Idle remains permanently selectable and consumes no application slot.
- EDF compares deadlines only inside its plugin.
- Release metadata changes only at policy release/registration boundaries.
- `saved_sp` remains at byte offset zero.
- All traversals are statically bounded; no dynamic allocation is introduced.

## Future admission control

The execution-budget placeholder permits a later, separately reviewed admission-analysis layer. Sprint 12 does not reject a valid task set based on utilization, deadline feasibility, response-time analysis, or overload behavior. Mixed policies, runtime policy switching, SMP, migration, and partitioning remain out of scope.
