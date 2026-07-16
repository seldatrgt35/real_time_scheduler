# Sprint 11 — Tickless Idle and Power Architecture

## Status and scope

Sprint 11 adds a dedicated portable power manager, coalesced kernel-time
advancement, deterministic host simulation, and an S32K148 LPTMR0/WFI target
path. It does not add a scheduling-policy interface, EDF, RMS, SMP, dynamic
allocation, MPU isolation, FPU context management, DVFS, or dynamic clock
scaling.

## Architecture

```text
idle task
   |
   v
portable power manager
   |  sleep limit / elapsed ticks / wake classification
   v
private port contract
   |
   v
S32K148 periodic SysTick + LPTMR0 one-shot + WFI
```

The scheduler owns task state, ready ordering, switch planning, and kernel
time. The power manager owns the decision to suppress ticks, the minimum
deadline calculation, hooks, wake validation, diagnostics, and the single
time-skip request. The architecture/target port owns interrupt masking around
WFI, periodic-source suppression and restoration, one-shot programming, wake
source classification, and hardware elapsed-time measurement. Portable files
do not include CMSIS or NXP headers and do not access SysTick, LPTMR, SCB,
NVIC, PCC, or the clock tree.

All memory remains static. The power manager adds no task, queue, heap,
recursive path, or unbounded search.

## Sleep eligibility and policy

`rts_power_sleep_is_allowed()` accepts sleep only when all of these facts are
true under a rechecked critical section:

- tickless idle is enabled at compile time;
- the kernel lifecycle is `RUNNING` in Thread mode;
- `current_task` is the private idle task in `RUNNING` state;
- no switch plan is pending or active;
- every ready FIFO is empty except priority zero, which contains exactly the
  idle task.

The ready queue remains the sole owner of intrusive ready-node mutation. The
power manager performs a read-only bounded scan through the configured
priority queues. A runnable timer-service task or any application task
therefore prevents sleep naturally.

Zero-duration plans are rejected. A port may report a zero-tick external wake;
that is a successful sleep entry but causes no time advance.

## Earliest wake calculation

The plan begins with a mandatory scheduler-maintenance deadline:

```text
maintenance = now + RTS_TICKLESS_MAX_SLEEP_TICKS
```

The power manager then chooses the wrap-safe minimum of:

1. the head of the ordered delay queue;
2. the head of the ordered active software-timer queue;
3. the maintenance deadline.

Synchronization timeouts share the delay queue and are therefore included
without a second scan or duplicated timeout index. The timer-service callback
queue does not contribute a deadline: when nonempty, its service task is
READY, so the sleep eligibility rule rejects tickless idle.

All candidate distances are within the unsigned 32-bit half range. An already
due head produces a zero-duration plan and leaves normal scheduler/tick
processing responsible for it. Wraparound uses the approved modular ordering;
there are no unrelated pointer-order comparisons.

## Tick suppression and target transaction

The portable transaction is:

1. test eligibility, enter the scheduler critical section, and recheck;
2. calculate the earliest bounded wake;
3. invoke `prepare_sleep()` and `before_sleep()`;
4. pass the maximum whole-tick sleep to `rts_port_power_sleep()`;
5. let the target stop the periodic tick, arm a one-shot source, atomically
   unmask interrupts and execute WFI;
6. return with scheduler interrupts masked, periodic timing restored, an
   elapsed-tick count, and a wake classification;
7. invoke `resume_from_sleep()`, advance time once, invoke `after_sleep()`,
   and leave the critical section;
8. issue at most one architecture switch notification if the normal
   scheduler selection created a switch plan.

The target result is rejected if elapsed time exceeds the requested bound or
the wake source is invalid. A port failure records an abort and advances no
kernel time.

The four hooks are weak no-op defaults. They run in the power transaction and
must be bounded and nonblocking. Board/application overrides may prepare
peripherals or record power telemetry, but may not mutate scheduler queues or
perform scheduling.

## S32K148 implementation

The normal running tick remains Cortex-M SysTick at `RTS_TICK_RATE_HZ`.
Tickless idle temporarily disables it and uses LPTMR0 as a one-shot source.
LPTMR0 uses the 1 kHz low-power clock, bypasses the prescaler, and is limited
to 16-bit durations; the S32K148 profile consequently sets the maintenance
bound to 60,000 ticks. Longer idle intervals are represented by repeated
bounded sleeps, not a wider or ambiguous modular deadline.

The target owns PCC clock gating, LPTMR registers, NVIC enable/priority,
SysTick suppression/restoration, PRIMASK handoff, and WFI. LPTMR0 IRQ 58 has a
dedicated vector. Its handler only acknowledges the timer and records the wake
classification; it does not advance time. For GPIO or another external IRQ,
the port latches the LPTMR count and returns the observed whole ticks. Thus the
portable kernel requires no knowledge of which peripheral woke the MCU.

This Sprint uses normal sleep, not deep-sleep clock-tree reconfiguration.
Board-specific deep modes may be added behind the same port contract only when
their retained clock, wake latency, and elapsed-time guarantees are defined.

## Time compensation and resynchronization

`rts_kernel_time_skip(elapsed)` shares the exact processing engine used by
`rts_kernel_tick_advance(elapsed)`. The two entry contracts differ only in
execution context: periodic tick entry requires ISR mode; tickless skip
requires Thread mode inside the power transaction.

One coalesced advancement performs:

- modular `current_tick` advancement;
- scheduler/runtime tick accounting;
- extraction of every due delayed task and synchronization timeout;
- software-timer expiration, periodic phase-preserving reschedule, and timer
  service wakeup;
- elapsed time-slice accounting where applicable;
- one final highest-ready selection and immutable switch-plan preparation.

No synthetic SysTick interrupts are replayed. Periodic software timers retain
their previous-expiration phase and use the existing missed-period accounting,
so a long sleep does not accumulate callback drift. Idle runtime is also
correct because the idle task remains the running task across the skipped
interval and its stopped/snapshot accounting observes the advanced tick.

## Diagnostics

Runtime diagnostics add saturating counters for sleep attempts, accepted
entries, aborts, timer wakes, external/GPIO/other wakes, suppressed ticks, and
the longest sleep. The diagnostic snapshot exposes entry count, suppressed
ticks, and longest sleep. Trace adds bounded `SLEEP_ENTER`, `SLEEP_EXIT`, and
`SLEEP_ABORT` events; no trace storage is allocated when tracing is disabled.

The S32K148 smoke record exposes before/after hook counts and compensated
elapsed ticks. Existing task, timer, stack, PSP/MSP, register-preservation,
and invariant probes remain active while lower-priority tasks now block for
short intervals so the idle task can reach WFI.

## Complexity and determinism

| Operation | Bound |
|---|---:|
| sleep eligibility | O(`RTS_PRIORITY_COUNT`) |
| delay deadline lookup | O(1) |
| software-timer deadline lookup | O(1) |
| plan selection | O(1) after eligibility |
| target arm/resume | O(1) |
| time compensation | O(expired tasks + expired timers + ready bitmap), all statically bounded |

The maximum number of expired tasks is bounded by the application pool plus
private tasks. The maximum number of expired software timers is
`RTS_MAX_TIMERS`. No loop depends on untrusted memory growth.

## Verification

Focused host tests cover maintenance-only planning, delayed-task precedence,
software-timer precedence and phase-preserving reschedule, timer and external
wakes, zero/one-tick sleeps, wraparound, the 60,000-tick maximum interval,
port failure without time mutation, and a deterministic 512-interval
pseudo-random simulation. The simulation checks the exact accumulated tick
after every wake, detecting missed wakeups and cumulative delay drift.

The S32K148 target test uses a CMSIS device mock to verify PCC/LPTMR setup,
NVIC ownership, WFI entry, LPTMR timer wake, early external wake, elapsed count,
SysTick restoration, and masked return. ARM-target compilation checks the new
target C and vector assembly under the Cortex-M4 soft-float ABI.

## Remaining limitations

- Wake compensation has whole-tick resolution; sub-tick energy/latency
  profiling is target diagnostics, not scheduler time.
- The S32K148 path uses LPTMR0 and normal WFI sleep. STOP/VLPS modes require a
  separately reviewed clock-retention and wake-latency contract.
- There is no runtime power-policy registration; Version 1 uses one
  compile-time policy and optional weak hooks.
- Deadline calculation supports existing fixed-priority delays, timeouts, and
  software timers. Future EDF/RMS policy events must contribute through a
  reviewed scheduler-maintenance deadline rather than accessing hardware.
