# Sprint 7 Integration and Acceptance Review

**Review date:** 2026-07-16
**Scope:** Sprint 7A, Sprint 7B, and Sprint 7C
**Software status:** ACCEPTED
**Hardware evidence:** PENDING ON S32K148

## 1. Accepted runtime baseline

Sprint 7 completes the Version 1 portable runtime scheduling model:

```text
fixed-priority preemption
+ equal-priority FIFO round robin
+ relative delayed blocking
+ tick-driven ordered wakeup
+ idle fallback
+ PendSV-only execution transfer
```

Scheduler time is a private `uint32_t` modulo clock. Relative intervals and
elapsed calls are limited to the strict half range. No implementation-defined
unsigned-to-signed narrowing or raw unsigned deadline ordering remains.

## 2. Ownership review

| Concern | Accepted owner | Review result |
| --- | --- | --- |
| Global tick and slice counters | Portable scheduler | PASS |
| Delay ordering and delay-node mutation | Delay queue | PASS |
| Ready membership, bitmap, FIFO rotation | Ready queue | PASS |
| Task state and wait metadata | Scheduler orchestration | PASS |
| Switch plan, coalescing, immutable snapshot | Switch planner | PASS |
| Register transfer | Cortex SVC/PendSV port | PASS |
| SysTick registers/vector/priority | S32K148 target | PASS |
| Public API | Existing five Version 1 functions | PASS |

The target timer contains no task, queue, priority, or slice policy. Portable
modules include no NXP header. Queue links are changed only through their owning
APIs. Context transfer remains deferred to PendSV.

## 3. Integrated tick transaction

The accepted order is tick advance, wake-all, slice accounting/optional single
rotation, one final selection, one plan operation, and at most one port
notification. Multiple expirations preserve delay FIFO and ready-tail order.
Simultaneous wakeup and slice expiry select the highest final priority without
an intermediate transfer.

Runtime bounds are O(k) for `k` due tasks and otherwise constant/bounded bitmap
work, with `k <= RTS_MAX_TASKS`. No full task-pool scan or unbounded
elapsed-based rotation exists.

## 4. Fixed priority and fairness

The review confirms:

- higher numeric application priority always wins;
- equal-priority wakeup alone does not preempt;
- lower-priority wakeup never preempts a RUNNING higher task;
- quantum expiry never admits a lower-priority task without an equal peer;
- one equal-priority peer causes one head-to-tail rotation;
- disabled slicing performs no tick rotation, while public yield still rotates;
- idle is never sliced and is immediately preempted by an application wake.

The elapsed-greater-than-one policy is intentionally coalesced: a call may
expire the current quantum but performs at most one fairness rotation and reloads
the full configured quantum. Exact multi-quantum fairness after long masking is
not claimed.

## 5. State and switch review

Both outgoing contracts pass:

- runnable outgoing: RUNNING to READY;
- delayed outgoing: BLOCKED remains BLOCKED.

Incoming is READY to RUNNING/current and receives a full quantum. The special
delay-expiry-before-PendSV race restores the still-executing current task to
RUNNING and cancels its stale pending blocking plan. ACTIVE snapshots remain
immutable and later work uses deferred reselection.

## 6. Lifecycle and context review

- RESET: tick source disabled and tick entry invalid;
- INITIALIZED: SysTick configured but disabled;
- RUNNING: SVC safely commits SysTick while PRIMASK is set;
- rollback: timer stops and lifecycle/current ownership return coherently;
- SysTick: private ISR entry only, then optional single PendSV request;
- public yield/delay: ISR rejected;
- task delay: non-idle RUNNING task context only.

## 7. Configuration matrix

| Configuration | Result |
| --- | --- |
| Slicing enabled, quantum 1 | PASS |
| Slicing enabled, quantum 5 | PASS |
| Slicing enabled, quantum 10, assertions disabled | PASS |
| Slicing disabled | PASS |
| Small pool / 8 priorities | PASS |
| Larger pool / 16 or 65 priorities | PASS |

The public configuration checks Boolean slicing and nonzero enabled quantum;
the private contract additionally verifies quantum representation in
`rts_tick_t`. The architecture-visible saved-SP TCB offset remains zero and is
unchanged by configuration.

## 8. Verification evidence

The available environment executed deterministic freestanding tests for tick
helpers, delay queue, task delay, wakeup/preemption, yield, scheduler start,
switch planning, Cortex bridge, SysTick mock integration, and the slicing matrix.
A 2,000-event round-robin stress test plus 500 mixed delay/wakeup/slice cycles
across tick wrap verify current/state, ready and delay membership, queue counts,
pool allocation count, and inactive plan after every completed event.

Strict-warning portable builds pass. Cortex-M4 Thumb soft-float debug and
optimized compilation passes for the changed portable units and smoke register
probe. Disassembly inspection finds no VFP operations. Static target checks
continue to require one handler, no heap/host symbols, PSP operations, and the
approved SVC instruction.

CMake itself, the real NXP CMSIS device package, and physical S32K148 hardware
were unavailable in this workspace. Consequently a real SDK-backed final link,
map verification, and on-target timing/fault evidence remain external acceptance
items; they are not falsely reported as passed.

## 9. Hardware acceptance procedure

Flash the three-task smoke image and confirm:

1. the sampled tick advances at 1 kHz;
2. A wakes every ten ticks and preempts B/C;
3. B/C counters alternate under the five-tick quantum without explicit yield;
4. PSP is used by A/B/C and handler probes use MSP;
5. wake and rotation observations increase;
6. stack guards and R4-R11 checks remain clean;
7. the target HardFault record remains clear; and
8. idle does not run while B or C is READY.

## 10. Feature-boundary confirmation

Sprint 7 adds no mutex, semaphore, event, queue, software timer, absolute sleep,
tickless idle, dynamic allocation, runtime priority change, task deletion,
public current-tick query, ISR-safe public task API, EDF/RMS policy, or FPU
context support.

## Approved Sprint 7 Runtime Baseline

Version 1 uses a private 32-bit wrap-safe scheduler clock, relative delays up to
`INT32_MAX`, ordered delayed wakeup, higher-priority preemption, optional
compile-time equal-priority round robin, full-quantum reload on every RUNNING
adoption, and one coalesced PendSV notification per scheduling transaction.
Queue, scheduler, port, and target ownership boundaries are accepted without an
open software architecture blocker.

Sprint 7 software is accepted. Physical S32K148 evidence remains mandatory
before target release qualification.
