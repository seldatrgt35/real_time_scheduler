# Sprint 9 Kernel Diagnostics

## Scope and ownership

Sprint 9 adds a private, statically allocated observability and reliability
layer. It does not add scheduling policy, public diagnostics functions, heap
use, callbacks, formatted output, or target headers to portable code.

```text
kernel operations ---> counters / trace ---> private snapshots and tests
       |                    |
       +-- invariant checks +-- fixed overwrite-oldest ring
       |
assert / corruption ---> central fatal record ---> weak target hook ---> halt
       |
HardFault capture (S32K148 target only)
```

The scheduler continues to own task state, current task, and switch plans. The
diagnostics layer observes those objects and updates counters only at existing
transaction boundaries. It never chooses a task or mutates queue ordering.

## Configuration

Every selected `rts_config.h` explicitly defines and validates these Boolean
options:

- `RTS_ENABLE_DIAGNOSTICS`
- `RTS_ENABLE_TRACE`
- `RTS_ENABLE_STACK_GUARDS`
- `RTS_ENABLE_STACK_WATERMARK`
- `RTS_ENABLE_RUNTIME_STATS`
- `RTS_ENABLE_INVARIANT_CHECKS`

`RTS_TRACE_CAPACITY` and `RTS_STACK_GUARD_SIZE_BYTES` are also explicit. The
debug and S32K148 profiles enable all features with 64 trace entries and a
16-byte guard. The release profile disables them; the minimal fatal record and
explicit fatal path remain available because kernel corruption is not a
recoverable condition. Disabled trace call sites compile to `((void)0)` and
optional TCB fields are removed.

## Fatal and assertion architecture

`rts_kernel_fatal_at()` is the authoritative production path. It disables
interrupts through the selected port, captures the first failure in volatile
kernel-owned storage, invokes the weak `rts_target_fatal_hook()` only for that
first failure, and never returns. The record is bounded and contains numeric
reason, lifecycle, tick, exception number, current/context addresses, source
pointer/line, and switch-plan generation/flags. No strings are copied.

The default weak `rts_assert_fail()` enters this path. Tests may replace that
weak function so invalid internal transactions can be observed without
terminating the process. Public invalid arguments still return public status
codes. Stack corruption found at a switch boundary uses the explicit
stack-corruption fatal reason even when assertions are disabled.

The target hook executes after interrupts are disabled and must not call
scheduler APIs. The S32K148 hook copies only bounded scalar evidence to its
debugger-visible target record. The task-return traps use the central fatal
path and do not imply task deletion.

## Stack guards, bounds, and watermark

Application stack buffers retain their public base-and-byte-count meaning.
With guards enabled, the lowest 16 bytes of each downward-growing stack are
reserved and filled with `0xA5`; validation increases the minimum required
buffer size by those bytes. The idle stack uses the same preparation path.
The remainder is filled with `0xCD` before the architecture creates its initial
frame at the high end.

Guard checks are fixed O(guard size) and run at creation/bootstrap validation,
switch completion, explicit invariant checks, and target checkpoints. Saved SP
validation checks the guarded lower bound, exclusive upper bound, and 16-byte
alignment. For a RUNNING target task this validates the stored suspension SP;
portable code never reads PSP. The S32K148 smoke record separately proves PSP
use and can be extended with a port-private live-SP observation if required.

Watermark measurement scans untouched fill bytes from the usable low end and
reports maximum observed used bytes. It is O(stack size), is never performed
on every production switch, and is private to tests/examples. It must not be
called from a high-priority ISR.

## Runtime accounting and snapshots

Kernel counters cover ticks, switch requests/completions, starts, creations,
yields, delay block/wake, semaphore block/acquire/timeout, mutex
block/handoff/timeout, and priority raise/restore. Per-task optional fields are
dispatch, block, wake, accumulated running ticks, last-start tick, and maximum
stack use.

Discrete event counters saturate at `UINT32_MAX`. Running time deliberately
uses unsigned modulo subtraction, so snapshots must be sampled within one
32-bit tick interval. Switch completion accounts the outgoing task once and
starts the incoming task once. First-task adoption initializes the same model.
Idle accounting is therefore identical to application accounting.

The private snapshot contains tick, context switches, task count, idle ticks,
non-idle ticks, and fatal reason. It takes a short port critical section and
exposes no TCB pointer. Before start and for a zero-duration interval, runtime
values are zero. Utilization is derived by callers from snapshot deltas using
integer arithmetic; no floating point or global counter reset is used.

## Trace

Trace is a fixed 64-entry ring in enabled profiles. Each entry contains a
saturating sequence, tick, event byte, and two `uintptr_t` numeric arguments.
Insertion is bounded O(1), protected by the existing single-core critical
section, and overwrites the oldest entry while saturating an overwrite count.
Reads take the same short protection. Task, tick ISR, semaphore ISR, and
portable switch-completion sites are supported. No application callback or
I/O occurs, including while a kernel lock is held.

## Invariant validation

The private validators perform bounded traversals and cover:

- pool slot states and allocated count;
- ready bitmap, list links, FIFO priority index, and duplicate bounds;
- delay ordering and delay-node ownership;
- READY/RUNNING/BLOCKED membership combinations;
- reachable semaphore/mutex waiter ordering and object validity;
- mutex owned-list reciprocity and effective-priority requirements;
- bounded wait-for traversal to reject priority-inheritance cycles;
- idle identity and readiness;
- current-task and PENDING/ACTIVE switch-plan transition states;
- stack guard and stored-SP bounds.

A current task may be BLOCKED only during a valid pending/active switch from
that task. Full validation is O(tasks + bounded queue memberships + reachable
synchronization objects). There is no global synchronization-object registry,
so initialized objects that are neither owned nor referenced by a waiter are
validated by their operation entry points rather than by `validate_all()`.
Full scans compile away when disabled.

## HardFault and target evidence

The S32K148 HardFault bridge captures EXC_RETURN, active MSP/PSP, the eight
basic stacked words, CFSR, HFSR, DFSR, SHCSR, MMFAR, and BFAR, then enters the
central fatal path. It cannot return or attempt recovery. MemManage, BusFault,
and UsageFault are not separately enabled in this sprint; their escalation to
HardFault is the documented Version 1 behavior.

The long-run smoke record now includes diagnostic tick/switch/runtime values,
per-task dispatch and high-water values, fatal reason, and invariant-failure
evidence. A target-test-only DWT module records the maximum outermost PRIMASK
critical window in raw core cycles. It records counter availability, remains
outside portable code and exception assembly, and cannot be mistaken for a
zero measurement when DWT is unavailable. Physical execution and deliberate
target fault injection remain required before target release qualification.

## Complexity and memory impact

Measured C11 layouts from Clang are:

| Item | ARM32 disabled | ARM32 enabled | Host64 disabled | Host64 enabled |
| --- | ---: | ---: | ---: | ---: |
| private TCB | 100 B | 128 B | 192 B | 216 B |
| per-TCB diagnostics delta | 0 B | 28 B | 0 B | 24 B |
| fatal record (always present) | 44 B | 44 B | 56 B | 56 B |
| trace entry | not linked | 20 B | not linked | 32 B |

On the ARM target, the three application TCBs therefore cost 384 bytes when
enabled versus 300 bytes when disabled; the separate idle TCB adds the same
28-byte delta. Runtime counters add 64 bytes. A 64-entry ARM trace ring uses
1,280 bytes plus 16 bytes of ring metadata. The guard consumes 16 bytes of
usable space in every configured stack without allocating separate RAM.
The S32K148 smoke-only DWT timing record adds 12 bytes.
`saved_stack_pointer` remains at offset zero, enforced by compile-time layout
assertions.

ARM public synchronization layouts remain 28 bytes per semaphore and 32 bytes
per mutex. The S32K148 smoke tasks each configure 1,024 bytes, reserve 16 bytes
for the kernel guard, and begin with a 64-byte Cortex-M4F frame. The smoke also
checks a conservative first 32 bytes. Actual maximum-use and remaining-margin
values are debugger-recorded and are marked unmeasured until hardware runs.

The target build runs the selected `size -A` tool after linking and writes
`rts_s32k148_smoke.size.txt`, so flash, RAM, `.bss`, and `.data` values come
from the current ELF rather than stale source constants.

## Verification and limitations

The focused diagnostic executable covers first-failure retention, synthetic
HardFault capture, guard/SP corruption, watermark, trace wrap/sequence,
runtime counters, ready/delay/mutex corruption, stale switch snapshots, valid
membership transitions, counter saturation, and 20,000 deterministic mixed
events with validation after every event. It runs with diagnostics enabled,
release diagnostics disabled, and time slicing disabled.

Clang strict-C11 compilation passes for all portable kernel sources in those
three configurations. Cortex-M4F soft-float freestanding syntax checks pass for
the diagnostics, port bridge, S32K148 target, and smoke sources. No SVC/PendSV
assembly was instrumented or changed. Physical long-run, target fault capture,
observed DWT critical-window values, context-switch/SVC timing, final ELF sizes,
and measured stack margins remain hardware/toolchain evidence, not software
claims.
