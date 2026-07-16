# Real-Time Scheduler — Version 1 Public API Design

**Status:** Approved public API baseline  
**Scope:** Public types and declarations only; no scheduler behavior is implemented by this document.

## 1. Design principles and header ownership

The API is deliberately small, uses only C11 and standard integer/size types, reports errors explicitly, allocates nothing, and exposes neither hardware concepts nor private task representation.

- `rts/rts.h` owns only the `rts_init()` and `rts_start()` lifecycle declarations. No stop, reset, deinitialize, state-query, or symmetric-but-unused lifecycle operation exists in version 1.
- `rts/rts_task.h` owns static creation, yield, and relative delay. There is no current-task query or identity accessor because version 1 has no API that needs a handle to the current task.
- `rts/rts_types.h` owns status, tick, priority, opaque handle, entry signature, task descriptor, the stack declaration macro, and public configuration validation.

## 2. Architecture amendment: kernel-owned static TCB pool

The original opaque byte-storage union was rejected during Sprint 2. Size and alignment checks cannot make an unrelated private `struct rts_task` overlay strictly conforming under the C11 declared-type and aliasing rules. Avoiding undefined behavior would have required an exposed layout, compiler aliasing extensions, or bytewise field encoding.

Static allocation remains mandatory, but ownership of task control blocks moves to a private compile-time pool of actual `struct rts_task` objects. `RTS_MAX_TASKS` is the exact application-task pool capacity. The separately allocated private idle task consumes no application slot. Application task stacks remain caller-owned, byte-counted, aligned static storage.

This amendment removes `RTS_TASK_STORAGE_SIZE`, `rts_task_storage_t`, `RTS_TASK_STORAGE_DECLARE`, and `rts_task_config_t.task_storage`. The five public function signatures are unchanged. Handles point directly to typed pool elements, so no reinterpretation cast is involved.

## 3. Stack-storage decision

The descriptor uses `void *stack_buffer` plus `size_t stack_size_bytes`. Byte sizing is architecture-neutral, works with static byte arrays, does not expose Cortex word width, and remains meaningful on a future 64-bit port. Stack direction and the initial exception frame are port-private.

`RTS_TASK_STACK_DECLARE(name, bytes)` creates a static byte array aligned to the version 1 portable contract of 16 bytes. This conservative, hardware-neutral public contract satisfies the initial target without naming Cortex concepts. The implementation validates address alignment, nonzero/minimum usable size, size alignment required by the selected port, and enough room for its private initial frame. There is no public stack-element type.

Applications may supply a manually declared buffer, but then must satisfy `RTS_TASK_STACK_ALIGNMENT`; the declaration macro is preferred. No compiler-specific attribute appears in the generic header: the macro uses standard C11 `_Alignas`.

## 4. Task creation descriptor

| Field | Ownership/lifetime | Nullability and validation | Stored behavior |
|---|---|---|---|
| `entry` | Function supplied by application; code lifetime is the image lifetime. | Must not be null. | Pointer is stored and becomes the initial task PC; it does not affect scheduling rank. Returning is outside the task contract and leads to the kernel fatal path. |
| `argument` | Application owns referenced data and must keep it valid for as long as the task can use it. | May be null. | Pointer value is copied into private task state/initial context; pointee is not copied. |
| `stack_buffer` | Application owns the storage; it must have static lifetime and must not be accessed as ordinary data after successful creation. | Must not be null; address, size, and non-overlap assumptions are validated where feasible. | Pointer is stored; contents are used for task context. |
| `stack_size_bytes` | Value is copied. | Must meet selected-port minimum and alignment constraints. | Bounds the private stack region; scheduling rank is unaffected. |
| `priority` | Value owned by the descriptor and copied at creation. | Must be in `1..RTS_PRIORITY_COUNT-1`; zero is reserved. | Determines fixed-priority ordering and is immutable in version 1. |

The descriptor itself is read only during `rts_task_create()` and may have automatic lifetime. Referenced stack storage must have system lifetime. No task-storage pointer, name, ID, deadline, period, budget, affinity, or future-policy field is present.

## 5. Task handle

`rts_task_handle_t` is a pointer to the incomplete public tag `struct rts_task`. It points directly to a typed private pool element and avoids casts, an ID table, generation counters, and integer/pointer conversions.

The null handle means “no task” and is never returned on successful creation. A successful handle remains stable for system lifetime. Applications may store, copy, compare for equality, and compare with null; they may not dereference it, perform pointer arithmetic, convert it to infer storage, or synthesize one. Assertion-enabled builds validate recognizable object state when a future handle-taking API is introduced. Version 1 exposes no such consumer other than returning the handle from creation.

## 6. Status and error model

`rts_status_t` contains: `OK`, `INVALID_ARGUMENT`, `INVALID_CONTEXT`, `INVALID_STATE`, `INVALID_TASK_CONFIG`, `INVALID_PRIORITY`, `INVALID_STACK`, `CAPACITY_EXHAUSTED`, `ALREADY_INITIALIZED`, `ALREADY_STARTED`, and `PORT_ERROR`.

Specific priority and stack statuses make static-integration faults actionable; `INVALID_TASK_CONFIG` covers invalid combinations not represented by one field. Capacity exhaustion means all `RTS_MAX_TASKS` pool slots are allocated. `PORT_ERROR` reports a recoverable architecture initialization or pre-transfer startup failure. Internal corruption never maps to a public status; it follows the fatal invariant path. `errno` is never used.

- `rts_init()` is not idempotent; a repeat returns `ALREADY_INITIALIZED` (or `ALREADY_STARTED` if running).
- `rts_start()` returns an error if preflight or architecture startup fails. On success it does not return.
- Yield and delay return status so invalid state/context is observable in non-assert builds.
- Task-only calls before start return `INVALID_STATE`.
- All public calls from ISR context return `INVALID_CONTEXT` when context detection is available; assertion builds also assert. Calling them from an execution environment where ISR context cannot be detected violates the documented precondition, but is never specified as valid behavior.

## 7. Lifecycle and creation timing

The public lifecycle has exactly three states:

```text
RESET --rts_init()--> INITIALIZED --rts_start()--> RUNNING
```

| Call | RESET | INITIALIZED | RUNNING |
|---|---|---|---|
| `rts_init` | Performs initialization | `ALREADY_INITIALIZED` | `ALREADY_STARTED` |
| `rts_task_create` | `INVALID_STATE` | Allowed | `INVALID_STATE` |
| `rts_start` | `INVALID_STATE` | Starts; success does not return | `ALREADY_STARTED` |
| `rts_task_yield` | `INVALID_STATE` | `INVALID_STATE` | Allowed from a running task; idle is a no-op |
| `rts_task_delay` | `INVALID_STATE` | `INVALID_STATE` | Allowed from a non-idle task |

All application tasks must be created after initialization and before start (Model A). This produces a closed task set, deterministic registration, no run-time creation critical path, and no creation-triggered immediate preemption. Supporting run-time creation later is an additive behavioral decision requiring a new review.

Starting with no application tasks is legal: the mandatory kernel idle task runs. There is no public STOPPED or FAILED state; an unrecoverable post-start fault is outside normal lifecycle control.

`rts_start()` selects the FIFO head at the highest ready priority without
rotating any queue, adopts it as the RUNNING current task, and then transfers to
the selected execution port. Idle is selected when no application task is ready.
Calls from ISR context return `INVALID_CONTEXT`; calls in RESET return
`INVALID_STATE`; calls after the RUNNING commit return `ALREADY_STARTED`.
Recoverable architecture rejection before execution transfer returns
`PORT_ERROR` and restores INITIALIZED state. On an embedded execution port,
successful transfer begins task execution and normally does not return.

## 8. Yield semantics

`rts_task_yield()` is task-context-only and RUNNING-only. The running task remains ready and is moved behind ready peers at the same priority. If no equal-priority peer exists, it remains selected and no lower-priority task runs solely because of yield. The idle task therefore observes a successful no-op. A successful yield resets that task's current time-slice accounting whether or not a peer exists, giving one consistent rule. Before start it returns `INVALID_STATE`; from ISR context it returns `INVALID_CONTEXT`.

## 9. Delay semantics

`rts_task_delay(delay)` measures a relative interval from the tick value observed atomically during the call. For `1..RTS_DELAY_MAX`, the current task becomes blocked and is eligible—not guaranteed—to run when that many scheduler tick boundaries have elapsed. Higher-priority work can delay its actual execution.

Zero has exactly yield semantics and returns the result of that operation. Values greater than `RTS_DELAY_MAX` return `INVALID_ARGUMENT`; limiting intervals to half the 32-bit range enables unambiguous modular wraparound comparison. Tick-counter wrap is handled internally and is invisible to the caller. Version 1 has no early wakeup, cancellation, absolute sleep, or spurious return: a successful nonzero delay returns only after expiry and redispatch. Calls before start return `INVALID_STATE`; ISR calls return `INVALID_CONTEXT`. The private idle task is not a public caller: an attempted idle delay is an internal invariant violation and returns `INVALID_STATE` after the configured assertion path.

## 10. Priority semantics

`rts_priority_t` is `uint8_t`. Numerically larger values represent higher task priorities. Zero is `RTS_IDLE_PRIORITY` and is kernel-reserved; applications use `1` through `RTS_PRIORITY_COUNT - 1`. `RTS_PRIORITY_COUNT` must be between 2 and 256. Constant expressions can be checked by application static assertions; every task creation is validated at runtime. Task priorities are unrelated to NVIC interrupt priorities.

## 11. Identity and names

Version 1 has no numeric ID and no task name. The opaque handle is the sole identity. Names are diagnostic metadata, would add a stored pointer and lifetime contract, and do not support baseline correctness. They can be introduced later behind a separately reviewed optional diagnostic facility.

## 12. Configuration interaction

`rts_types.h` directly includes exactly one selected `rts_config.h`; this prevents an application translation unit from using public types without the configuration that defines their contract. CMake supplies one configuration include directory to the entire final image and must reject multiple configuration providers.

Required macros are `RTS_MAX_TASKS`, `RTS_PRIORITY_COUNT`, `RTS_TICK_RATE_HZ`, `RTS_ENABLE_TIME_SLICING`, `RTS_TIME_SLICE_TICKS`, `RTS_IDLE_STACK_SIZE_BYTES`, and `RTS_ENABLE_ASSERTIONS`. Public preprocessing checks the API-affecting values; private headers check idle-stack, representation, and port constraints.

`rts_tick_t` is always `uint32_t`, priority is always `uint8_t`, and public stack alignment is always 16 bytes. The private TCB may vary with reviewed private configuration without changing public layout. Configuration contains no MCU clock, vector, CMSIS, or SDK value.

## 13. ISR, reentrancy, and concurrency contract

Version 1 exposes no ISR-callable function. `rts_init`, `rts_start`, and `rts_task_create` are startup-only and non-reentrant. `rts_task_yield` and `rts_task_delay` are RUNNING-only, task-context-only, preemption-safe, and callable by multiple tasks over time; each call operates on the caller. The implementation protects its short state transition internally, which is not permission for concurrent ISR invocation.

ISR misuse returns `INVALID_CONTEXT` where detectable and triggers an assertion when enabled. Documentation is the static prohibition; C11 cannot encode execution context in the function type. Misuse is not assigned useful scheduler behavior and no `_from_isr` aliases exist.

## 14. Illustrative use

```c
#include "rts/rts.h"
#include "rts/rts_task.h"

RTS_TASK_STACK_DECLARE(worker_a_stack, 512u);
RTS_TASK_STACK_DECLARE(worker_b_stack, 512u);

static void worker(void *argument);

static int scheduler_example(void)
{
    rts_task_handle_t worker_a;
    rts_task_handle_t worker_b;

    const rts_task_config_t a = {
        .entry = worker,
        .argument = NULL,
        .stack_buffer = worker_a_stack,
        .stack_size_bytes = sizeof worker_a_stack,
        .priority = 2u
    };
    const rts_task_config_t b = {
        .entry = worker,
        .argument = NULL,
        .stack_buffer = worker_b_stack,
        .stack_size_bytes = sizeof worker_b_stack,
        .priority = 2u
    };

    if (rts_init() != RTS_STATUS_OK) { return 1; }
    if (rts_task_create(&a, &worker_a) != RTS_STATUS_OK) { return 2; }
    if (rts_task_create(&b, &worker_b) != RTS_STATUS_OK) { return 3; }
    if (rts_start() != RTS_STATUS_OK) { return 4; }
    return 5; /* Unreachable after a successful start. */
}
```

## 15. Misuse examples

- `static unsigned char stack[513];` passed directly may be insufficiently aligned and its byte count may violate port constraints; runtime creation returns `INVALID_STACK`. The declaration macro makes address alignment compile-time-correct, while minimum/usable size remains runtime-validated.
- Creating more than `RTS_MAX_TASKS` application tasks returns `CAPACITY_EXHAUSTED`; no partial task is published.
- Calling `rts_task_create()` after `rts_start()` returns `INVALID_STATE`; in normal successful execution another task would have to make that call because start does not return.
- Calling `rts_task_delay()` in an ISR returns `INVALID_CONTEXT` where detectable and asserts in assertion builds.
- A descriptor with `.entry = NULL` returns `INVALID_TASK_CONFIG`.
- Priority zero or a value not below `RTS_PRIORITY_COUNT` returns `INVALID_PRIORITY`; a `_Static_assert` can detect a constant application priority earlier.

## 16. Implementation review checklist

- [ ] Only the three approved headers are installed publicly.
- [ ] No private, Cortex-M, CMSIS, NXP, queue, TCB, or context type leaks publicly.
- [ ] TCBs come only from the private static pool; stacks are never allocated internally.
- [ ] Storage/stack static-lifetime requirements are documented at every entry point.
- [ ] Every API follows the RESET/INITIALIZED/RUNNING table exactly.
- [ ] Creation is rejected outside INITIALIZED state.
- [ ] ISR misuse is checked before state mutation where the port can identify it.
- [ ] Every failure returns the documented status without partial registration.
- [ ] All translation units consume one identical `rts_config.h`.
- [ ] Every returned handle points directly to an allocated typed pool element.
- [ ] Opaque handles are stable and never dereferenced by application code.
- [ ] Stack bounds, alignment, minimum size, and uniqueness assumptions are validated.
- [ ] Yield never selects a lower-priority task solely due to yielding.
- [ ] Delay uses wrap-safe arithmetic and rejects values above `RTS_DELAY_MAX`.
- [ ] Public ABI constants/types change only through an explicit compatibility review.

## Sprint 8A synchronization amendment

Sprint 8A adds `rts_count_t`, `RTS_WAIT_FOREVER`, `RTS_STATUS_TIMEOUT`,
`RTS_STATUS_FULL`, `rts_semaphore_t`, and `rts_semaphore_init`,
`rts_semaphore_take`, `rts_semaphore_give`, and
`rts_semaphore_give_from_isr`. The semaphore is a caller-owned typed C11
object, not opaque byte storage. It must be zero-initialized static storage
before its one successful initialization and retain the same address
thereafter. Its intentionally public layout contains counters, opaque task
endpoints, bounded waiter count, and release validity metadata; it exposes
neither a TCB definition nor intrusive links. Copying, moving, or modifying an
initialized object is invalid; self identity makes copying detectable.

`RTS_WAIT_FOREVER` is the single reserved infinite timeout. Finite semaphore
waits use `0..RTS_DELAY_MAX`; other values are invalid. `TIMEOUT` represents
both immediate unavailability and elapsed timeout. `FULL` represents give at
maximum count. Maximum count one is the binary-semaphore model.

Only `rts_semaphore_give_from_isr` is ISR-callable. It never blocks or transfers
directly and returns the coalesced PendSV-notification decision. A
scheduler-aware ISR must be maskable by PRIMASK and request PendSV at most once
after aggregating its kernel operations. All preexisting APIs retain their
task-context rules.

Sprint 8B additionally adds the typed static `rts_mutex_t` and
`rts_mutex_init`, `rts_mutex_lock`, and `rts_mutex_unlock`. Mutexes are
non-recursive, task-owned, unavailable to ISR context, and use the same finite
and forever timeout representation. Self-lock and non-owner unlock return
`INVALID_STATE`; no extra public deadlock or ownership status is introduced.

## Approved Version 1 Public API Baseline

The exact public types are `rts_status_t`, `rts_tick_t` (`uint32_t`), `rts_priority_t` (`uint8_t`), incomplete-pointer `rts_task_handle_t`, `rts_task_entry_t`, and `rts_task_config_t` containing only entry, argument, byte-counted stack pointer/size, and priority.

The exact public functions are `rts_init`, `rts_start`, `rts_task_create`, `rts_task_yield`, and `rts_task_delay`; all return `rts_status_t`.

Task objects use a private typed static pool of `RTS_MAX_TASKS` elements; the private idle object is separate. Task stacks use caller-owned `void *` plus byte count with 16-byte alignment; `RTS_TASK_STACK_DECLARE` is the preferred declaration form.

Lifecycle is RESET → INITIALIZED → RUNNING. Initialization is one-shot, task creation is allowed only after initialization and before start, and successful start does not return. Starting with no application tasks is legal because the idle task exists.

Yield rotates only equal-priority peers, never admits lower-priority work solely due to yield, and resets slice accounting. A zero delay is yield. Nonzero delay is relative to call time, accepts at most `RTS_DELAY_MAX`, is wrap-safe, and has no early wakeup.

Priority zero is reserved for idle; applications use `1..RTS_PRIORITY_COUNT-1`; larger numeric values have greater scheduling priority. No public operation is ISR-safe, and ISR calls return `INVALID_CONTEXT` where detectable and assert when configured.

Every public translation unit includes the same selected `rts_config.h`. Configuration supplies application-pool capacity, priority count, tick frequency, time-slicing choice/quantum, and assertion choice. Public tick width and stack alignment are fixed; private TCB layout is not a public ABI dependency.
