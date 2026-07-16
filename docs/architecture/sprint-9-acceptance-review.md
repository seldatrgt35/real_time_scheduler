# Sprint 9 Diagnostics and Reliability Acceptance Review

**Decision:** CONDITIONALLY ACCEPTED. Static and host evidence is complete;
physical S32K148 long-run, fault-injection, size, stack-margin, and latency
evidence remains pending.

## Reviewed files and corrections

The review covers configuration/public type validation, TCB and kernel state,
fatal/assert paths, stack preparation and validation, runtime accounting,
trace, global invariants, scheduler/tick/synchronization integration, host
tests, Cortex-M4F bridges, S32K148 fault capture/smoke, build verification, and
documentation. Corrections made during acceptance include support for the
valid BLOCKED-current switch transition, bounded inheritance-cycle validation,
protected trace reads, saturating snapshot bounds, complete CMSIS test mocks,
and focused release-profile fault-injection behavior.

## Ownership and fatal sequence

```text
Scheduler / queues / sync objects       Cortex-M4F / S32K148
               |                                 |
      observation boundaries              HardFault evidence
               +---------------+-----------------+
                               v
                    private diagnostics layer
                 counters | trace | validators
                               |
 assertion / explicit corruption --------+
                               v
 disable interrupts -> first record -> target hook -> deterministic halt
```

Diagnostics never select tasks, mutate queues, request extra switches, or call
application code. The portable fatal record is first-failure-wins. The target
hook runs after masking and copies bounded evidence; the fatal path never
returns.

## Stack and runtime models

```text
low address                                      high address
+----------------+-------------------------+-------------------+
| 16-byte 0xA5   | 0xCD watermark region   | initial/use frame |
+----------------+-------------------------+-------------------+
^ stack_low                                  stack_high (exclusive)
```

The public buffer/byte-count contract is unchanged, while enabled diagnostics
reduce usable space by 16 bytes. The idle stack follows the same model.

```text
task becomes RUNNING: last_start = tick, dispatch++
switch completion:    outgoing += tick - last_start
                      incoming last_start = tick
snapshot:             stored + current elapsed; idle/non-idle by delta
```

All arithmetic is 32-bit integer. Event counters saturate; elapsed task time is
modulo and requires sampling within one tick wrap.

## Trace and invariant model

Trace uses a 64-entry static overwrite-oldest ring, O(1) critical-section
insertion, saturating sequence/overwrite counters, and no callbacks or I/O.
Disabling trace removes the ring and all emission calls.

| Task state | Ready | Delay | Object wait | Current |
| --- | --- | --- | --- | --- |
| READY | linked | no | no | no |
| RUNNING | linked | no | no | yes |
| DELAY blocked | no | linked | no | transition-only if outgoing |
| finite semaphore/mutex wait | no | linked | linked | transition-only if outgoing |
| forever semaphore/mutex wait | no | no | linked | transition-only if outgoing |

The validator additionally checks bitmap/list consistency, delay ordering,
pool counts, idle identity, switch-plan coherence, stack bounds, synchronization
wait ordering, owned-mutex reciprocity, effective priority, and bounded acyclic
inheritance chains.

## Host and static evidence

| Evidence | Result |
| --- | --- |
| Diagnostic/fault-injection test, enabled | PASS |
| Diagnostic/fault-injection test, assertions and diagnostics disabled | PASS |
| Diagnostic/fault-injection test, trace disabled/time slicing disabled | PASS |
| Deterministic stress events per profile | 20,000 PASS |
| Validation after every enabled-profile stress event | PASS |
| Delay, slicing, semaphore, and mutex regressions | PASS |
| Strict C11 all portable kernel sources, three profiles | PASS |
| Cortex-M4 soft-float diagnostics/port/target/smoke syntax | PASS |
| Saved-SP offset-zero assertions | PASS |
| S32K148 DWT availability/maximum-window host mock | PASS |
| Dynamic allocation / formatted-I/O dependency introduced | none |
| SVC/PendSV assembly modified by diagnostics | no |

The host stress sequence cycles tick advancement, yield/switch completion,
semaphore take/give, and mutex lock/unlock. Focused injection also covers ready
bitmap, delay membership, mutex ownership, stale switch generation, stack
guard/SP, nested fatal capture, and synthetic HardFault reason retention.

## RAM, ROM, TCB, and stack report

| Layout evidence | Diagnostics disabled | Diagnostics enabled |
| --- | ---: | ---: |
| ARM32 TCB | 100 B | 128 B |
| Host64 TCB | 192 B | 216 B |
| ARM32 application pool (`RTS_MAX_TASKS=3`) | 300 B | 384 B |
| ARM32 fatal record | 44 B | 44 B |
| ARM32 runtime counters | 0 B | 64 B |
| ARM32 trace ring + metadata (`capacity=64`) | 0 B | 1,296 B |
| S32K148 smoke-only DWT timing record | 0 B | 12 B |
| ARM32 semaphore / mutex | 28 B / 32 B | unchanged |

The separate idle TCB has the same per-TCB delta and the configured idle stack
is 512 bytes. Each smoke task stack is 1,024 bytes: 16-byte diagnostic guard,
64-byte initial frame, and 944 bytes initially available beyond those fixed
reservations. Actual watermark margin is pending hardware observation. Target
post-link tooling now emits section-by-section flash/RAM output; final ROM,
`.data`, and `.bss` numbers require the NXP device package and bare-metal linker
run and are intentionally not fabricated.

## Hardware evidence matrix

| Target evidence | Status |
| --- | --- |
| Strict target source/ABI static verification | PASS |
| Long-run mixed task/delay/slicing/semaphore/mutex smoke | instrumented, not run |
| Fatal reason and invariant failure volatile fields | implemented, not observed |
| Per-task dispatch/high-water and idle/runtime fields | implemented, not observed |
| Deliberate UsageFault/HardFault PC/register capture | implemented, not injected |
| Physical PSP/MSP and context preservation | pending Sprint 6-8 hardware gate |
| Context-switch/SVC/yield latency | not instrumented; not measured |
| Outermost PRIMASK critical-window latency | DWT hook implemented; not measured |

No WCET or hardware-completion claim is made. The smoke-only DWT hook records
raw maximum masked cycles and availability without touching exception assembly.
Future GPIO or capture-timer switch measurements must remain target-test-only
and report the exact build and clock profile.

## Remaining risks

- Physical S32K148 execution and fault evidence is absent.
- No global registry exists for dormant synchronization objects; validation is
  complete for reachable objects and operation entry points.
- Runtime and utilization deltas must be sampled within the documented 32-bit
  modulo window and before saturating global counters lose interval detail.
- Stack watermark is pattern-based evidence, not proof against arbitrary
  in-range corruption.
- Timing hooks and measured interrupt/context-switch latency remain target work.
- The repository remains non-certified and requires board clock, linker,
  startup, safety, and production fault-policy review.

## Sprint 10 assumptions

Software Timers may rely on the stable wrap-safe tick model, scheduler-owned
wakeup/preemption, static object lifecycles, task/wait membership states,
central fatal handling, trace events, bounded invariant scans, and deterministic
host stress infrastructure. Sprint 10 must keep timer objects statically owned,
define callback context explicitly, avoid executing application callbacks
inside tick/critical sections, and integrate with the existing absolute-time
ordering without changing the public task-delay semantics.

## Acceptance checklist

- [x] Explicit validated diagnostics configuration
- [x] Central non-returning fatal path and first-failure record
- [x] Assertion, task-return, and HardFault integration
- [x] Application and idle stack guards, bounds, and watermark
- [x] Kernel/task/synchronization runtime counters and private snapshot
- [x] Fixed-capacity ISR-safe trace ring and disabled build
- [x] Full bounded invariant framework and membership matrix
- [x] Fault injection and 20,000-event deterministic host stress
- [x] Diagnostics-enabled/disabled and slicing-disabled tests
- [x] Strict portable and ARM32 static verification
- [x] Generated target size-report tooling and measured type layouts
- [ ] Physical long-run S32K148 smoke and fault injection
- [ ] Target stack margins, final ELF sizes, and latency measurements

## Approved Sprint 9 Diagnostic Baseline

Version 1 diagnostics are private and consist of one central fatal record/path,
fixed stack guards and on-demand watermarking, bounded runtime counters and
snapshots, a compile-time removable fixed trace ring, bounded global invariant
validation, and S32K148 HardFault evidence capture. Scheduler decisions and the
public API remain unchanged. Software integration is accepted; physical target
qualification remains conditional.
