# Sprint 8 Synchronization Acceptance Review

**Decision:** ACCEPTED for software integration; physical S32K148 evidence is
conditional and remains pending.

## Reviewed scope and corrections

The review covers public types/headers, TCB metadata, wait object, semaphore,
mutex, priority inheritance, ready/delay integration, tick dispatch, planner,
host port/tests, S32K148 tick boundary/smoke, build lists, and Sprint 8
documentation. Review corrections introduced a shared typed wait-storage
component, explicit base/effective priority, waiter reprioritization, bounded
owned-mutex count, prospective cycle detection, multi-owned restoration, and
one final selection after priority decrease.

## Architecture and ownership

```text
Public semaphore / mutex API
              |
              v
Synchronization orchestration
       |                 |
  Wait-object      Priority inheritance
       |                 |
       +--------+--------+
                v
      Scheduler/task state
       |       |        |
    Ready Q  Delay Q  Switch planner
                         |
                       PendSV
```

- wait object alone mutates `wait_node`;
- ready queue alone mutates `ready_node`;
- delay queue alone mutates `delay_node`;
- mutex alone mutates owner and owned links;
- priority layer alone changes effective priority;
- scheduler owns state/current/plans;
- port alone requests the ordinary PendSV transfer.

## State and ownership sequences

```text
RUNNING --contended lock/take--> BLOCKED
  ready linked                  object-wait linked
                                delay linked iff finite

BLOCKED --handoff/give--> READY --PendSV completion--> RUNNING
BLOCKED --timeout-------> READY --selection-----------> RUNNING
```

```text
unowned mutex --lock--> owner L --H waits--> owner L(effective=H)
owner L --unlock--> direct owner H + former L restoration
```

Transitive sequence: H6 waits on M's mutex, M3 waits on L's mutex, so M and L
both become effective 6. L releases to M, then M releases to H; each release
recomputes all remaining ownership requirements before scheduling.

## Membership matrix

| Task form | Ready | Delay | Object wait | Owned mutex |
| --- | --- | --- | --- | --- |
| RUNNING/READY | yes | no | no | zero or bounded many |
| DELAY blocked | no | yes | no | zero or bounded many |
| semaphore forever | no | no | semaphore | zero or bounded many |
| semaphore finite | no | yes | semaphore | zero or bounded many |
| mutex forever | no | no | mutex | zero or bounded many |
| mutex finite | no | yes | mutex | zero or bounded many |

An owned mutex is linked to exactly one owner. An unowned mutex is unlinked and
has no waiters. One task never reuses a node across structures.

## Lifecycle and context matrix

| API | RESET/INITIALIZED | RUNNING task | ISR |
| --- | --- | --- | --- |
| semaphore/mutex init | allowed before use | allowed only for unused object | invalid |
| semaphore take/give | invalid state | allowed | invalid |
| semaphore give-from-ISR | invalid state | invalid context | allowed |
| mutex lock/unlock | invalid state | allowed | invalid |

Initialization is one-shot, static-address-only, and has no destroy. Runtime
task creation/deletion remains absent.

## Scheduling and race review

Effective priority indexes all ready FIFOs and slicing groups. Priority changes
relocate through queue APIs and blocked waiters are deterministically reordered.
Higher wake/inheritance and restoration use one final selection. PENDING plans
retarget without duplicate notification; ACTIVE snapshots remain immutable and
defer reselection. PendSV remains the sole transfer.

Give/timeout and unlock/timeout serialize under PRIMASK. The winner removes all
competing memberships, writes one result, and inserts READY once. Semaphore
count is unchanged by direct handoff. Mutex owner changes directly without an
ownerless stealing window. Semaphores never inherit.

## Complexity and deterministic bounds

Immediate semaphore operations and uncontended lock are O(1). Wait insertion
is bounded O(`RTS_MAX_TASKS`); removal/cancellation/handoff is O(1). Priority
recomputation is O(`RTS_MAX_MUTEXES_PER_TASK`) per owner, with transitive depth
bounded by `RTS_MAX_TASKS`. Tick expiry remains O(k) due tasks plus these
bounded restorations. No common-path task-pool scan, heap, recursion, or SMP
lock exists.

## Verification matrix

| Evidence | Result |
| --- | --- |
| Semaphore init/count/direct handoff/timeout/ISR/races | PASS |
| Mutex ownership/misuse/direct handoff | PASS |
| Basic, multiple-owned, transitive inheritance | PASS |
| Timeout restoration and wraparound | PASS |
| Cycle detection and waiter reprioritization | PASS |
| Assertions enabled/disabled, slicing enabled/disabled | PASS |
| 2,000 semaphore + 1,000 nested mutex cycles | PASS |
| Delay, slicing, task-create, bootstrap, selection, planner regressions | PASS |
| Strict C11 portable compilation | PASS |
| Cortex-M4 Thumb soft-float/no-FPU compilation | PASS |
| Saved-SP offset zero static assertion | PASS |
| Cortex TCB / three-slot application pool | 104 bytes / 320 bytes |
| Physical S32K148 execution | PENDING |

Static review finds no allocation API, ISR mutex API, target header in portable
synchronization, or change to SVC/PendSV assembly. The existing assembly files
are unmodified in Sprint 8.

## Hardware evidence procedure

On S32K148 verify low mutex acquisition, high blocking, low execution ahead of
medium under inheritance, direct high ownership handoff, finite restoration,
ISR semaphore acquisition, semaphore timeout, R4–R11/PSP/MSP evidence, intact
stack guards, and clear HardFault record. No hardware pass is claimed here.

## Remaining risks and Sprint 9 readiness

Public synchronization objects intentionally expose stable metadata and depend
on documented no-copy/no-mutation discipline. Maximum owned mutex count defaults
to task capacity and should be explicitly sized in a production configuration.
Priority inheritance is single-core PRIMASK-based and has no owner-death policy
because deletion is absent. Full deadlock graph diagnosis is not implemented;
prospective chain cycles are rejected and corruption cycles are fatal.

Sprint 9 Kernel Diagnostics may rely on stable TCB lifecycle/stack bounds,
task/wait states, synchronization invariants, tick/context switching,
fatal/assert hooks, and deterministic host fixtures. Diagnostics must not alter
public object storage or scheduling semantics.

## Acceptance checklist

- [x] Static strict-C11 semaphore and mutex objects
- [x] Finite/forever wait and ISR-safe semaphore give
- [x] Non-recursive task-owned mutex
- [x] Effective-priority ready/slice scheduling
- [x] Transitive bounded inheritance and restoration
- [x] Direct handoff and serialized timeout arbitration
- [x] Queue/node ownership preserved
- [x] PendSV-only context transfer and notification coalescing
- [x] Host stress and ARM/static verification
- [ ] Physical S32K148 evidence

## Approved Sprint 8 Synchronization Baseline

Sprint 8 software is accepted. Version 1 synchronization consists exactly of
binary/counting semaphores, finite/forever waits, ISR-safe semaphore give,
non-recursive mutexes, bounded transitive priority inheritance, deterministic
priority/FIFO waiters, direct handoff, and wrap-safe timeout arbitration. Target
release qualification remains conditional on physical S32K148 evidence.
