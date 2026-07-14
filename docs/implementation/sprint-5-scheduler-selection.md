# Sprint 5.2 — Scheduler Selection and Current-Task Ownership

**Status:** Implemented; ready for review  
**Scope:** Portable highest-ready selection and initial current ownership

## Approved running representation

Sprint 5.2 preserves the accepted model: READY and RUNNING tasks both remain linked in the ready set. `RUNNING` identifies the one ready member that owns execution; after scheduler start it must equal `current_task`. Selection never removes or rotates a task and may return the existing current task when it remains highest.

Idle remains permanently ready-linked at priority zero. It is selected naturally by the same ready bitmap when no application priority is populated; there is no separate fallback policy or special queue.

## Private contracts

| Operation | Responsibility | Mutation |
|---|---|---|
| `rts_scheduler_select_highest_ready()` | Return the highest runnable member from the ready owner | None |
| `rts_scheduler_task_is_idle(task)` | Recognize only the exact private idle object/pointer | None |
| `rts_scheduler_current_get()` | Read scheduler-owned current pointer | None |
| `rts_scheduler_current_is_valid()` | Validate lifecycle/current/state/ready invariants | None |
| `rts_scheduler_current_establish(task)` | Establish the selected initial current task | READY→RUNNING and `current_task` assignment only |
| `rts_scheduler_current_release_initial()` | Roll back a pre-transfer initial establishment | RUNNING→READY and clear `current_task` only |

The release operation exists for a future recoverable `rts_start()` failure before execution transfer. It is INITIALIZED-only and performs no runtime switch or queue mutation.

## Selection validation

Selection is legal in INITIALIZED, for first-task preparation, and RUNNING, for later scheduler decisions. RESET selection is an internal contract violation. The ready owner supplies the highest member; the scheduler verifies:

- nonnull saved SP, stack bounds, and entry;
- ready-set membership;
- delay-node non-membership and wait NONE;
- READY state, or RUNNING only when it is exactly `current_task`;
- ALLOCATED ownership metadata;
- valid application-pool identity and application priority, or exact private idle identity at priority zero;
- assertion magic when enabled.

A missing ready member after successful bootstrap is an unconditional fatal invariant because the idle task must always exist.

Application identity uses the existing bounded pointer-equality pool scan. Idle recognition compares only against both `idle_task` and `&idle_task_storage`; no priority-zero application object is accepted.

## Current-task ownership

Before start, INITIALIZED has a null current pointer and every runnable task is READY. Initial establishment requires:

1. lifecycle INITIALIZED;
2. current pointer null;
3. the supplied task is exactly the current highest-ready selection;
4. the task is READY and runnable.

The scheduler changes that task to RUNNING and assigns `current_task`. Its ready link, FIFO position, bitmap bit, wait metadata, saved SP, and slice value remain unchanged.

This creates a short pre-start transitional state where lifecycle is still INITIALIZED but current is valid and RUNNING. A future `rts_start()` must perform establishment, lifecycle publication, and port transfer under its critical transaction. If the port reports failure before transfer, it uses `rts_scheduler_current_release_initial()` to restore READY/null before leaving INITIALIZED.

After lifecycle becomes RUNNING, current must be nonnull, RUNNING, ready-linked, and runnable. Runtime replacement remains owned by the future switch-plan commit implementation and is not implemented here.

## Concurrency and ownership

These functions do not enter critical sections themselves and make no port call. Their caller must hold the kernel critical contract whenever lifecycle or ready/current state could change concurrently. This avoids hidden nested masking and lets a later start or switch transaction protect all related state with one token.

The ready queue remains the sole owner of intrusive links, FIFO order, bitmap, and highest-member lookup. Scheduler selection reads those structures and owns only state/current invariants. No function requests switching, calls PendSV/SVC, starts a task, rotates a queue, advances time, blocks, or wakes a task.

## Complexity

Highest selection retains the ready-set bound of O(`RTS_READY_BITMAP_WORDS`), at most eight 32-bit words for Version 1, plus O(`RTS_MAX_TASKS`) application identity validation. Idle validation is O(1). Establishment and release add only constant state mutations; establishment includes one selection.

No memory is allocated and no new kernel-global storage is introduced.

## Tests

Focused host tests cover:

- idle-only selection after bootstrap;
- read-only selection with null current;
- mixed-priority highest selection;
- equal-priority FIFO head selection;
- application current establishment;
- idle current establishment;
- RUNNING task remaining ready-linked and selectable;
- unchanged ready counts and lower-task states;
- current invariant validation;
- pre-start current release and READY/null restoration;
- assertion rejection of a non-highest candidate, duplicate establishment, and RESET selection.

Tests run under both assertion configurations. They perform no task execution or scheduler start.

## Remaining Sprint 5 work

- `rts_start()` must wrap initial establishment/lifecycle transfer in the critical contract.
- Switch-plan preparation/acquire/commit must own runtime RUNNING↔READY replacement.
- Yield, delay, tick/preemption, context-switch requests, and host/target execution remain absent.
