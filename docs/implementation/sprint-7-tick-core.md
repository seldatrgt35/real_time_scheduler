# Sprint 7A — Portable Tick Core and S32K148 Tick Source

## Scope and ownership

Sprint 7A adds scheduler time without adding blocking, wakeup, preemption, or
time slicing. `rts_kernel_state_t.current_tick` is the single writable clock.
Only `kernel/time.c` updates it; the target sees the read-only private accessor
and calls the kernel-owned advancement entry. No public time API was added.

Bootstrap zero-initializes the tick with the rest of kernel state. Failed
bootstrap restores the whole state to RESET and therefore restores tick zero.
Task creation and scheduler start do not reset time.

## Tick type and modulo arithmetic

`rts_tick_t` remains exactly `uint32_t`, verified with a compile-time assertion.
All arithmetic is unsigned modulo 2^32. The private helpers are:

- `rts_tick_before(a, b)`;
- `rts_tick_reached(now, deadline)` (after-or-equal within the valid window);
- `rts_tick_elapsed(start, end)`; and
- `rts_tick_relative_is_valid(relative_ticks)`.

`before` subtracts modulo 2^32 and tests the high bit. It never converts an
out-of-range unsigned value to a signed type. Equality is not before. Exactly
`0x80000000` ticks of separation is ambiguous, so neither operand is considered
before the other. Relative intervals and one advancement call are consequently
limited to `0x7fffffff`. Elapsed time is the defined unsigned difference and is
meaningful only when the caller's observation interval respects its contract.

## Portable tick entry

`rts_kernel_tick_advance(elapsed_ticks)` is the future tickless seam. The
periodic source passes one; a future one-shot source may pass a bounded elapsed
count without changing target ownership. Sprint 7B refines its return value to
`switch_notification_required`; the target requests PendSV only when that value
is true. A call is valid only when:

- lifecycle is RUNNING;
- the caller is in a scheduler-aware ISR;
- elapsed is nonzero; and
- elapsed is at most `0x7fffffff`.

Invalid internal use asserts and returns without mutation. In Sprint 7A a valid
call performed only the modulo addition. Sprint 7B supersedes that limitation by
extracting due delayed tasks and preparing wakeup preemption, while the target
still owns the final port notification. RESET, INITIALIZED, and Thread mode
calls are invalid. `rts_kernel_tick_now()` returns the value by copy.

## Selected S32K148 source

Version 1 uses Cortex-M SysTick, not LPIT/PIT. SysTick is part of the Cortex-M4F
core, already represented in CMSIS and in the core vector table, needs no PCC or
peripheral driver, and is sufficient for the initial periodic scheduler clock.
There is exactly one strong `SysTick_Handler` in the target image.

The standalone S32K148 image retains the reset/run FIRC core-clock policy and
declares `RTS_S32K148_CORE_CLOCK_HZ = 48,000,000`. With
`RTS_TICK_RATE_HZ = 1,000`, the exact integer period is 48,000 core cycles and
the programmed 24-bit reload is 47,999. The calculator rejects zero inputs,
non-integral division, rates above the source clock, and periods outside the
24-bit SysTick range. No rounding or hidden fallback is permitted. Integrations
that change the startup clock must update and validate this target declaration.

## Initialization and start ordering

Target port initialization calculates and writes LOAD, clears VAL, programs the
SysTick exception priority, reads configuration back, and leaves CTRL disabled
in READY state. A failure makes `rts_init()` return `RTS_STATUS_PORT_ERROR` and
kernel bootstrap rolls back.

`rts_start()` adopts the selected task, makes lifecycle RUNNING, and calls
`rts_port_tick_start()` while PRIMASK is set. That call clears VAL and moves
READY to ARMED but deliberately does not set ENABLE or TICKINT. Failure stops
the source, releases current-task ownership, restores INITIALIZED, and permits
retry.

The Cortex startup handoff is then prepared and SVC is triggered. The SVC
consumer validates and invalidates the handoff while still masked, calls
`rts_port_tick_commit_start()`, and only then restores the first task. Commit
enables CLKSOURCE, TICKINT, and ENABLE. PRIMASK remains set until the approved
SVC assembly restores startup interrupt semantics, so no tick can execute in
the vulnerable handoff window. The host port models readiness/start/stop
deterministically and does not execute tasks.

## ISR and interrupt priorities

The S32K148 four-bit logical priorities are frozen as:

| Exception | Logical | Encoded | Rule |
| --- | ---: | ---: | --- |
| SVC | 13 | `0xD0` | Higher urgency than scheduler-aware tick work |
| SysTick | 14 | `0xE0` | Maskable kernel-aware periodic interrupt |
| PendSV | 15 | `0xF0` | Lowest urgency; all switching remains deferred |

This refines the pre-tick S32K148 assignment from SVC 14 to SVC 13; the portable
SVC/PendSV ABI is unchanged. Future kernel-aware interrupts must obey the same
PRIMASK critical-section contract and may not have lower urgency than PendSV.

`SysTick_Handler` reads CTRL to acknowledge/observe COUNTFLAG and calls
`rts_kernel_tick_advance(1)`. It performs no floating-point work, public API
call, ready/delay mutation, selection, PendSV invocation, or direct switch.

## Missed ticks and wrap

Version 1 uses missed-tick Model A: every serviced SysTick exception advances
exactly one tick. SysTick does not provide a count of all periods accumulated
during long PRIMASK intervals, so the kernel does not claim wall-time accuracy
after such masking. Natural `0xffffffff` to zero wrap is valid and does not
reset queues or scheduler state.

## Verification

Focused host tests cover helper boundaries, equality, wrap ordering, exact
half-range ambiguity, elapsed calculation, one/multiple advancement, wrap,
invalid lifecycle/context/count calls, assertion-enabled and release behavior,
and preservation of current task, ready/delay counts, and switch-plan data.

Target-facing tests cover exact reload calculation, rejected rates, encoded
priority, disabled-before-start state, READY→ARMED→RUNNING ordering, stop,
handler acknowledgement path, and exactly one portable tick callback. Post-link
verification requires exactly one SysTick handler and tick-entry symbol, rejects
LPIT/PIT handlers, host/heap symbols, and VFP instructions. Target compilation
continues to use Cortex-M4/Thumb/soft-float/no-FPU flags.

The smoke application copies the private tick accessor into
`g_rts_smoke.observed_tick` while both application tasks continue explicit
yielding. This is target-test-local evidence and is not a public API. Physical
S32K148 execution was not available in this workspace; debugger confirmation of
increasing samples, PSP task execution, MSP exception execution, and absence of
HardFault remains required.

## Remaining Sprint 7B dependencies

Sprint 7B will consume these contracts to implement public relative delay,
delay-queue insertion/extraction, READY/BLOCKED transitions, wakeup selection,
and deferred PendSV requests. It must preserve the strict half-range bound and
must define how multi-tick advancement extracts all due tasks without moving
that policy into the target ISR. Time slicing remains Sprint 7C.
