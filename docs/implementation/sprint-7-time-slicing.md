# Sprint 7C — Tick-Driven Equal-Priority Time Slicing

## Scope and configuration

Sprint 7C completes Version 1 scheduler time with compile-time-selected
equal-priority round robin. `RTS_ENABLE_TIME_SLICING` remains a strict Boolean;
when enabled, `RTS_TIME_SLICE_TICKS` is nonzero and must fit in the 32-bit
`rts_tick_t` slice field. No runtime policy selector or public API was added.

When slicing is disabled, tick advancement, delayed wakeup, fixed-priority
preemption, and voluntary yield remain operational. Tick processing neither
reads the slice value for policy nor rotates a FIFO. The private field stays in
the fixed TCB and is initialized deterministically from the configured value.

## Slice ownership and counter model

The portable scheduler owns counters. The ready queue alone rotates FIFO links,
the planner owns switch state, and the target ISR only acknowledges SysTick,
calls the private tick entry once, and optionally requests PendSV once.

A non-idle task receives a full quantum when:

- its private TCB is initialized;
- it is first adopted by `rts_start()`;
- it blocks and later wakes;
- it voluntarily yields;
- its slice expires; and
- it becomes incoming RUNNING at switch completion.

Idle has the same deterministic field initialization but is never decremented
or rotated. A delayed current awaiting PendSV is BLOCKED and is not charged;
if that same task wakes before transfer, its newly reloaded quantum is also not
charged for the elapsed interval in which it was logically blocked.
An ACTIVE snapshot is immutable and is not charged by a normally masked tick.

## Tick sequence

One tick entry performs this sequence:

1. validate RUNNING lifecycle, ISR context, and elapsed bound;
2. advance the modulo-2^32 scheduler tick;
3. extract and wake every due delay-queue head;
4. account the actual RUNNING current task's slice;
5. rotate its priority FIFO at most once if the quantum expired with a peer;
6. select the final highest READY task once;
7. prepare, coalesce, cancel, or defer one switch plan;
8. return one Boolean port-notification decision.

Wakeup and quantum expiry on the same entry therefore cannot publish an
intermediate equal-priority hardware transfer. A higher-priority awakened task
wins the single final selection even if the current FIFO was rotated first.

## Arithmetic and elapsed coalescing

Slice arithmetic is unsigned:

```text
elapsed < remaining  -> remaining -= elapsed
elapsed >= remaining -> quantum expires; remaining = configured quantum
```

The elapsed call may be larger than one quantum, but Version 1 deliberately
performs at most one rotation and discards additional fairness epochs for that
entry. It does not loop by elapsed ticks or perform multiple rotations. This is
the bounded coalesced-time model and is consistent with the SysTick missed-tick
limitation. Delay expiry still evaluates every deadline reached by the final
tick.

If no same-priority peer exists, expiry reloads the current quantum without
rotating, selecting a lower-priority task, creating a plan, or requesting the
port. Fixed priority always dominates fairness.

## Eligibility and pending plans

Slice accounting requires a RUNNING, non-idle current task at the head of its
priority FIFO, a valid nonzero counter, no ACTIVE snapshot, and enabled slicing.
Rotation additionally requires a same-priority peer.

The current task remains physically executing until PendSV completion. If an
equal-priority slice plan is already PENDING, its FIFO has already rotated and
the current task is no longer the head, so another tick cannot rotate it again.
A higher-priority wakeup may retarget the pending plan while preserving the
actual outgoing task and existing hardware notification. ACTIVE plans only set
deferred reselection; the Cortex bridge completes the immutable transfer before
requesting any later PendSV.

## State and queue invariants

At slice expiry, `rts_ready_rotate()` moves only the current ready node from
head to tail. The task remains RUNNING/current until completion, the selected
head remains READY, queue count and bitmap do not change, delay membership is
untouched, and at most one plan is published. Completion applies the established
rules:

- runnable outgoing RUNNING becomes READY;
- blocking outgoing remains BLOCKED;
- incoming READY becomes RUNNING/current with a full quantum.

Equal-priority wakeup appends at the FIFO tail and does not immediately preempt.
Its first opportunity is the current task's later yield or quantum expiry.

## Complexity

- modulo tick and slice accounting: O(1);
- due extraction: O(k), `0 <= k <= RTS_MAX_TASKS`;
- each ready insertion and one optional rotation: O(1);
- final fixed-priority selection: bounded by the configured bitmap width;
- switch preparation/coalescing: O(1).

There is no task-pool scan, ready-task traversal, dynamic allocation, or
elapsed-dependent rotation loop.

## Host and target verification

Focused tests cover:

- enabled quantum 1 and quantum 5;
- assertion-enabled and assertion-disabled configurations;
- slicing disabled while voluntary yield still rotates;
- partial decrement and exact expiry;
- elapsed larger than remaining with one rotation;
- no-peer expiry with a lower-priority READY task;
- two/three-task FIFO progression;
- simultaneous higher-priority wakeup and slice expiry;
- counter initialization at creation, start, wake, yield, expiry, and switch-in;
- delay/wakeup/preemption with slicing disabled; and
- 2,000 deterministic rotations with membership, queue, pool, and plan checks;
  plus 500 high-priority delay/wakeup cycles interleaved with lower-priority
  slicing across tick wrap.

Portable units compile under strict C11 warnings. Debug and optimized Cortex-M4
Thumb/soft-float objects for tick, delay, and the register probe contain no VFP
instructions. Previous tick, delay, yield, switch-plan, Cortex bridge, target
timer, and scheduler-start regressions remain passing in the available
freestanding host harness.

## S32K148 smoke behavior

The smoke configuration now enables a five-tick quantum and three static tasks:

- A, priority 3, validates execution state and delays ten ticks;
- B and C, priority 2, run continuously without explicit yield.

A's wake preempts B/C. While A is delayed, SysTick-driven rotation alternates B
and C. Target-local volatile observations include tick, A/B/C counters, current
test identifier, A wake count, observed B/C rotation count, PSP/MSP/CONTROL,
argument values, register failures, and stack-guard failures. The existing
target fault record remains the HardFault evidence. None is public API.

Physical S32K148 execution was not available in this workspace. Frequency,
long-run alternation, fault absence, and stack/register evidence must therefore
be confirmed on hardware before target-level release qualification.
