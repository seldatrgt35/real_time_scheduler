# SMP Preparation Boundary

**Release:** v1.0.0
**Status:** Architecture seam only; this release does not support SMP.

## Non-goals and specialization

`RTS_CPU_COUNT` is mandatory and v1.0.0 rejects every value other than one.
`rts_cpu_id_t` is private, CPU zero is the only valid identifier, and the
current-CPU query is a compile-time constant. There are no CPU loops, CPU
arrays, atomics, spinlocks, IPIs, affinity fields, migration code, or remote
cache operations in the release image.

The private `rts_cpu_local_state_t` groups the current task, idle-task pointer,
and switch plan. A C11 union keeps the established private field spellings
available to focused tests while providing one CPU-local view. Portable
scheduler writes use CPU-local accessors. Assembly still sees only the saved-SP
at TCB offset zero and therefore does not know about CPU-local state.

## CPU-local and global state

| State | v1.0.0 owner | Future SMP classification |
| --- | --- | --- |
| current task | CPU-local | one per CPU |
| switch plan/snapshot lifecycle | CPU-local | one publication channel per CPU |
| idle task pointer | CPU-local | one idle task per CPU/domain |
| task pool | kernel global | domain/global allocation authority |
| FP/RMS/EDF ready structure | selected policy, global | scheduler-domain owned |
| delay queue and scheduler tick | kernel global | time-domain owned |
| timer manager/callback queue | kernel global | serialized service owner |
| synchronization objects | object owner | object-local lock required |
| diagnostics aggregates | kernel global | atomic or per-CPU reduction required |

Interrupt nesting remains a port concern. Runtime-accounting timestamps are
currently stored per task; a future migrating RUNNING task requires a stopped
accounting transition before ownership changes.

## Lock model

Portable shared-state mutations use `rts_kernel_lock_enter/exit`. On v1.0.0
this wrapper is exactly one PRIMASK critical token and preserves exact nesting
and restore semantics. It contains no spin, retry, atomic, or cache operation.

A real SMP implementation must replace the lock implementation, not merely set
`RTS_CPU_COUNT` above one. Required design work includes interrupt masking
before lock acquisition, acquire/release barriers, a non-recursive ownership
rule, deterministic lock ordering, ISR rules, bounded contention analysis, and
separate locks for domains, time, timers, and synchronization objects. Lock
order must prevent a mutex-inheritance walk from reversing object/domain lock
order.

## Reschedule and future IPI model

The kernel requests `rts_port_request_reschedule(cpu)`. CPU zero maps to the
existing PendSV-set operation. The Cortex assembly helper only sets PENDSVSET;
it performs no scheduling decision. A future port may issue an IPI for a remote
CPU, but must publish the destination switch/reschedule state with release
ordering before notifying and must acquire it in the remote handler. No fake
IPI implementation exists in this release.

## Memory-ordering requirements for future SMP

Single-core PRIMASK critical sections sequence all current mutations, so C11
atomics are intentionally absent. Future SMP requires explicit ordering at:

- task state before ready-set publication;
- switch-plan contents before pending notification;
- callback item contents before ring-index publication;
- semaphore count/direct-handoff and waiter removal;
- mutex ownership before inheritance propagation;
- timeout-versus-give arbitration;
- migration ownership before destination insertion; and
- reschedule state before an IPI.

Compiler barriers alone are insufficient for those future cases. The existing
Cortex DSB/ISB instructions remain limited to exception/control-register
requirements and are not presented as SMP data barriers.

## Scheduler domains and policies

A future scheduler domain owns a CPU set, exactly one policy instance and ready
structure, affinity rules, and migration rules. v1.0.0 has one implicit domain,
one CPU, one compile-time-selected policy, and one global ready state. The
scheduler core continues to own lifecycle, task state, blocking/wakeup,
current-task adoption, switch planning, and port notification. Policy plugins
own ready membership, ordering, selection, yield rotation, release metadata,
tick policy work, and validation.

No affinity field is added to the TCB. It would consume deterministic RAM while
all legal masks equal CPU zero. A future ABI-reviewed private field or external
static task table is preferred when domains exist.

## Migration boundary

Future migration requires a task that is not RUNNING, removal under the source
domain lock, validation of wait/timeout and mutex-owner constraints, ownership
and affinity update, insertion under the destination domain lock, then
reschedule requests for affected CPUs. A blocked task remains owned by its
timeout and wait object; migrating a mutex owner also requires a defined
cross-domain inheritance protocol. None of these operations is implemented.

## Synchronization review

Semaphore/mutex count, owner, waiter list, timeout arbitration, and inheritance
mutations currently share one kernel-lock boundary. ISR give defers execution
through the same switch-plan/reschedule path. Future SMP requires object locks,
remote wakeup, waiter publication ordering, cross-CPU inheritance propagation,
owner-migration constraints, and a single winner for timeout versus give.
Classic priority inheritance is coherent for FP and RMS. EDF ready ordering is
deadline-based while mutex waiter ordering and inheritance use effective fixed
priority; this is a documented v1 limitation, not deadline inheritance.

## Timer and tickless review

v1.0.0 owns one tick, delay queue, timer manager, callback queue, timer-service
task, target timer, and sleep decision. Future SMP must choose a timekeeper CPU,
serialize callbacks, coordinate the earliest wake across domains, and prevent
one CPU from suppressing the global tick while another requires it. Per-CPU
idle does not by itself authorize package sleep.

## Blockers before real SMP

Real SMP remains blocked until domain storage, affinity, object/domain lock
ordering, atomics and barriers, IPI delivery, remote switch publication,
cross-CPU timeout arbitration, inheritance semantics, timer ownership,
coordinated tickless sleep, migration tests, race analysis, target hardware,
and multicore WCET evidence are designed and accepted.
