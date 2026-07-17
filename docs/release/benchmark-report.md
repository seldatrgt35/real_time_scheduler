# v1.0.0 Benchmark, Memory, and Stack Report

## Scope and evidence level

This report freezes the measurable software characteristics of the v1.0.0
release candidate. Host results establish functional determinism; they are not
Cortex-M4F WCET evidence. No physical S32K148 board was available for this
release review, so cycle counts, interrupt latency, power consumption, and
hardware stack high-water values remain explicitly unmeasured.

## Algorithmic bounds

| Operation | Bound | Bounded by |
| --- | ---: | --- |
| FP highest-ready selection | O(1) | priority bitmap word count fixed by configuration |
| FP ready insertion/removal | O(1) | intrusive links |
| RMS selection | O(1) | precomputed static priority plus FP mechanism |
| EDF highest-ready selection | O(1) | ordered queue head |
| EDF insertion | O(`RTS_MAX_TASKS`) | ordered intrusive traversal |
| Delay/timer insertion | O(capacity) | ordered static queue traversal |
| Task/timer pool allocation | O(capacity) | bounded first-free scan |
| Semaphore/mutex waiter selection | O(`RTS_MAX_TASKS`) | bounded waiter chain |
| Tickless elapsed-time advance | O(expired work) | static tasks, timers, and callback capacity |
| Global invariant validation | O(tasks + timers + queues) | compile-time capacities |

No operation allocates heap memory, recurses, or performs an unbounded retry.

## ARM private-object layout

Clang 22.1.8 was invoked for `arm-none-eabi`, Cortex-M4, Thumb, soft-float,
using `targets/nxp/s32k148/config`. The reproducible probe is
`tools/layout_probe.c`.

| Private object | Size | Alignment/notes |
| --- | ---: | --- |
| `struct rts_task` | 156 bytes | 4-byte ARM ABI alignment; saved SP at offset zero |
| `rts_cpu_local_state_t` | 24 bytes | current, idle, immutable switch-plan state |
| `struct rts_timer` | 88 bytes | diagnostics enabled in target configuration |
| `rts_kernel_state_t` | 2,384 bytes | configuration-dependent aggregate |

These are private ABI measurements, not public storage promises. Diagnostics,
capacity, and policy configuration may change the private sizes.

## Target static stack budget

The S32K148 reference configuration reserves:

| Stack | Bytes |
| --- | ---: |
| MSP exception/startup stack | 4,096 |
| Idle task | 512 |
| Timer service task | 768 |
| Each smoke-test application task | 1,024 |

The three-task smoke image therefore declares 3,072 application-stack bytes
and 8,448 total explicit stack bytes. Each application stack is caller-owned;
the total production budget depends on application task count and sizes. Stack
guards reserve 16 bytes inside each configured task region.

## Critical-section review

The portable kernel enters critical regions only through `rts_kernel_lock_t`.
For v1.0.0 this maps to the existing interrupt-mask token and preserves exact
nested restore semantics. Queue and ownership changes are completed before the
token is restored. The wrapper creates a future lock-domain boundary; it does
not claim SMP mutual exclusion.

Longer bounded paths inside critical sections include ordered EDF/delay/timer
insertion, timeout wake processing, priority-inheritance propagation, and
global validation when enabled. Hardware cycle measurements are required before
assigning a safety or hard real-time WCET budget.

## Host stress result

The deterministic seed `0x13a5c7e9` drives 50,000 events per policy over eight
tasks. FP, RMS, and EDF runs cover random block/unblock/yield activity,
wrap-proximate release/tick values, reselection, and invariant validation. All
three release-built executables returned success during the Sprint 13 gate.

## Required hardware benchmark procedure

Before promoting the candidate to an evidence-complete hardware release:

1. build FP, RMS, and EDF S32K148 images with the documented ARM toolchain;
2. measure scheduler, PendSV, SVC, SysTick, wakeup, and tickless-resume cycles
   with DWT CYCCNT and a GPIO correlation point;
3. record minimum/mean/maximum across at least one million switches and ticks;
4. force maximum configured ready, delay, timer, and waiter populations;
5. record MSP and every PSP high-water value after the stress interval;
6. measure idle current and wake latency for timer and external-interrupt wake;
7. archive compiler version, ELF/map file, probe firmware, board revision, and
   raw measurements with the release evidence.

Until those measurements exist, no empirical WCET, latency, or power claim is
made.
