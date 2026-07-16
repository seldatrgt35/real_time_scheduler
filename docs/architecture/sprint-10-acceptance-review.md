# Sprint 10 Acceptance Review

## Decision

Sprint 10 software timers are **ACCEPTED for the Version 1 baseline**, subject
to the physical-hardware evidence explicitly listed as pending below. The
portable architecture is complete for static registration, active scheduling,
deferred callback execution, periodic coalescing, diagnostics, and target-image
integration.

## Reviewed artifacts

- Sprint 3 list/delay contracts and Sprint 7 elapsed-tick ordering;
- Sprint 4 static TCB ownership and Sprint 5 selection/switch planning;
- Sprint 6 Cortex-M4F SVC/PendSV contracts;
- Sprint 8 semaphore/mutex and priority-inheritance contracts;
- Sprint 9 fatal, trace, runtime, stack, and invariant contracts;
- Sprint 10A timer pool/active queue implementation;
- Sprint 10B callback ring, service task, tests, and S32K148 smoke changes.

## Corrections made during review

1. Periodic active state was separated from callback PENDING/RUNNING state;
   Model A cannot be represented correctly by one mutually exclusive enum.
2. Timer service was made a private typed TCB and stack outside
   `RTS_MAX_TASKS`, like idle but initially BLOCKED.
3. Generation invalidation was paired with bounded queued-work removal so a
   restart cannot be delayed by a stale one-pending slot.
4. Queue-empty cancellation withdraws a READY service task and coalesces a
   pending switch, preventing a workless service dispatch.
5. Callback invocation was moved after critical exit and guarded by service
   current-task and non-ISR checks.
6. Blocking callback APIs were explicitly rejected instead of leaving
   serialized-service latency unbounded.

## State model

```text
timer lifecycle:  UNINITIALIZED -> STOPPED <-> ACTIVE
                                      ^          |
                         one-shot expiry/stop ---+

callback work:    IDLE -> PENDING -> RUNNING -> IDLE
                    ^        |
                    +-- generation invalidation
```

A periodic timer may be `ACTIVE+PENDING` or `ACTIVE+RUNNING`. A one-shot after
expiry is `STOPPED+PENDING/RUNNING`. These combinations are intentional and
validated.

## Callback flow

```text
ISR acknowledges hardware
  -> rts_kernel_tick_advance(elapsed)
  -> due active timers extracted in deadline/FIFO order
  -> periodic future deadline reinserted
  -> one typed work item enqueued when IDLE
  -> blocked service made READY once
  -> one final scheduler selection/switch plan
  -> PendSV transfers to private service task
  -> work dequeued under lock
  -> lock released
  -> callback invoked in Thread mode on PSP
  -> completion state committed under lock
  -> queue drained, service blocks, scheduler resumes selected task
```

## Lifecycle matrix

| API/condition | RESET | INITIALIZED | RUNNING task | ISR |
| --- | --- | --- | --- | --- |
| timer init | rejected | allowed | rejected | rejected |
| start/stop/restart | rejected | allowed | allowed | rejected |
| is-running | false | query | query | false |
| callback execution | none | none | service task only | forbidden |

Armed INITIALIZED timers use tick-zero-relative deadlines and do not progress
until the RUNNING target tick source advances time.

## Ownership matrix

| Component | Exclusive responsibility |
| --- | --- |
| Timer manager | pool, lifecycle, generation, active node |
| Active queue | deadline/FIFO ordering and node mutation |
| Callback ring | work indices, FIFO storage, maximum depth |
| Tick layer | expiry orchestration and one final selection |
| Scheduler | service TCB state, ready membership, preemption |
| Timer service | typed callback invocation and serialization |
| Target ISR | hardware acknowledgement and portable tick entry |
| Application | callback code and argument lifetime |

## Callback-context acceptance rules

Callbacks execute serially in ordinary privileged Thread mode using PSP. They
never execute in the timer ISR, PendSV, SVC, or under the timer critical lock.
Timer controls, semaphore give, nonblocking acquire/lock, unlock, and yield are
accepted. Positive delay and potentially blocking semaphore/mutex acquisition
are rejected with `RTS_STATUS_INVALID_CONTEXT`. Callback WCET and service
priority interference remain application analysis inputs.

## Simultaneous-event ordering

One elapsed-tick transaction performs:

1. advance global time;
2. wake due delayed/waiting tasks;
3. expire timers and publish callback work;
4. wake timer service at most once;
5. account the current time slice;
6. select the highest READY task;
7. prepare/coalesce one switch plan.

The target requests PendSV once from the returned notification. Timer count
does not multiply switch requests.

## Periodic, missed-period, and overflow policy

- Deadline-relative Model A reload is accepted.
- Elapsed jumps coalesce missed periods and use division to select the first
  future deadline without iteration.
- At most one pending/running callback exists per timer; later expirations are
  diagnosed as overrun and coalesced.
- Queue capacity is compile-time and at least timer capacity.
- Overflow is fatal configuration/corruption evidence; no callback is silently
  overwritten.
- Stop/restart increments generation and removes queued stale work. A running
  callback completes, but no not-yet-started stale callback executes.

## Verification evidence

Host evidence:

- strict C11 warnings-as-errors: diagnostics, release, no-slicing;
- focused timer suites in all three profiles;
- deterministic 50,000-event timer/service stress with full invariant checks;
- kernel bootstrap, scheduler start, semaphore, mutex, and diagnostics
  regression suites;
- callback critical depth zero and service current-task identity assertions.

ARM/static evidence:

- all portable kernel, Cortex-M4F C port, S32K148 target, and smoke C sources
  pass strict ARMv7E-M soft-float syntax verification;
- `-mfpu=none` prevents VFP generation in the verified compilation;
- `struct rts_task.saved_stack_pointer` remains offset zero;
- SVC/PendSV assembly files are unchanged;
- one target tick ISR remains authoritative;
- callback invocation exists only in `timer_service.c`, not tick or target ISR.

Hardware evidence matrix:

| Evidence | Status |
| --- | --- |
| Target image contains timer smoke instrumentation | implemented |
| callback PSP and zero IPSR capture | implemented, board run pending |
| periodic cadence observed | board run pending |
| one-shot semaphore wake observed | board run pending |
| no HardFault/stack-guard failure | board run pending |
| queue overflow absent over long run | board run pending |

No physical S32K148 result is claimed.

## Memory and complexity acceptance

The diagnostics-enabled target configuration uses an 88-byte timer, 16-byte
work item, 144-byte eight-entry ring, 868-byte complete manager, 128-byte
service TCB, and 768-byte configured service stack. Active insertion is bounded
O(`RTS_MAX_TIMERS`), head lookup and ring operations are O(1), expiry is O(k),
stale purge is bounded O(queue capacity), and validation is statically bounded.

## Remaining risks

- Callback WCET and selected service priority can interfere with application
  deadlines.
- Generation wrap relies on the bounded no-work-survives-2^32-invalidations
  assumption.
- Physical timing, stack margin, and long-duration queue evidence are pending.
- There is no callback cancellation acknowledgement or runtime period change.

## Sprint 11 readiness

The repository is architecturally ready to begin Tickless Idle and Power
Management design. Sprint 11 may rely on ordered delay and active-timer heads,
elapsed-tick advancement, a stable idle task, target timer abstraction,
diagnostics, and working SVC/PendSV transitions. It must define how the earliest
task or timer deadline programs low-power wakeup and how callback backlog
constrains sleep. No tickless behavior is implemented by Sprint 10.

## Acceptance checklist

- [x] no heap or dynamic nodes;
- [x] private service outside application capacity;
- [x] callback ring distinct from active intrusive node;
- [x] no callback in ISR/SVC/PendSV/critical section;
- [x] exact one-shot and deadline-relative periodic behavior;
- [x] bounded coalescing and one-pending overrun policy;
- [x] generation/stale protection;
- [x] one-notification tick integration;
- [x] diagnostics, trace, invariant, host stress, and ARM static checks;
- [x] S32K148 smoke support;
- [ ] physical-board evidence captured.
