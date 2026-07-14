# Sprint 2 — Version 1 Internal Kernel Design

**Status:** Approved internal baseline  
**Scope:** Private declarations and behavioral contracts only; no production implementation or assembly.

## 1. Storage and type model

The kernel owns one statically allocated `rts_kernel_state_t`. It contains an `application_task_pool` object whose fixed `slots[RTS_MAX_TASKS]` array holds the application TCBs, plus a separate `idle_task_storage`. Every pool element is an actual `struct rts_task`; `rts_task_handle_t` points directly to it. There is no overlay, unrelated cast, alias extension, or public/private size bridge.

Sprint 4.1 refines slot ownership to the explicit lifecycle `FREE -> RESERVED -> ALLOCATED`. Static zero initialization makes every slot `FREE`. `RESERVED` is the private, unpublished task-creation transaction state. Commit changes it to `ALLOCATED`; rollback changes it to `FREE`. Version 1 has no deletion, so an `ALLOCATED` slot remains allocated for system lifetime. This refinement supersedes the earlier two-state encoding without changing the public API.

The first-free search begins at the pool-owned `next_free_hint` and examines at most `RTS_MAX_TASKS`, wrapping once. Rollback moves the hint to the newly freed slot. This bounded traversal is deterministic. The pool-owned `allocated_count` equals the number of committed application slots, excludes reserved slots and idle, and never exceeds `RTS_MAX_TASKS`.

## 2. Private TCB

The normative definition is in `kernel/task_internal.h`. Field order is deliberate:

| Field | Owner / mutation owner | Initial value | Scheduling effect | Release requirement |
|---|---|---|---|---|
| `saved_stack_pointer: void *` | Port saves/restores; task module initializes | Port-produced initial SP | Execution transfer | Yes; offset zero |
| `stack_low/high: unsigned char *` | Task module, then immutable | Validated half-open bounds | None directly | Yes |
| `entry: rts_task_entry_t` | Task module, then immutable | Descriptor entry | Initial execution | Yes |
| `argument: void *` | Task module, then immutable | Descriptor argument | Initial execution | Yes |
| `ready_node: rts_list_node_t` | List owns links; ready queue owns object association | All fields null | Ready membership/FIFO | Yes |
| `delay_node: rts_list_node_t` | List owns links; delay queue owns object association | All fields null | Delay ordering | Yes |
| `wait: rts_wait_t` | Scheduler changes reason; delay contract uses wake tick | `NONE`, tick zero | Blocking eligibility | Yes |
| `slice_remaining: rts_tick_t` | Scheduler tick/yield logic | Configured quantum | Equal-priority rotation | Yes, even when slicing is disabled |
| `priority: rts_priority_t` | Task module, then immutable | Validated descriptor value | Fixed-priority selection | Yes |
| `state: rts_task_state_t` | Scheduler core exclusively | `DORMANT`, then `READY` | Eligibility | Yes |
| `slot_state: rts_task_slot_state_t` | Task pool manager | `FREE`, `RESERVED`, then `ALLOCATED` | Handle validity/capacity | Yes |
| `validation_magic: uint32_t` | Task creation publication/reset | Zero while FREE/RESERVED; magic only after commit | None | Assertions only |

Keeping `slice_remaining` unconditional prevents time-slicing configuration from changing TCB layout. List-node owner pointers are unconditional so singleton membership is independently testable; opaque object backlinks provide strict C11 object recovery without subtracting from a member pointer. Optional future diagnostics may add conditionally compiled private fields, but each addition must document deterministic pool RAM and may never move `saved_stack_pointer` from offset zero.

With ordinary ABI alignment and assertions disabled, the expected TCB size is approximately 68 bytes on a 32-bit ARM EABI build and 120 bytes on a typical 64-bit host. Enabling the 32-bit validation magic raises the estimates to approximately 72 and 128 bytes respectively. These are design estimates, not ABI constants; supported builds verify actual `sizeof` and `offsetof` values and record pool RAM in the link map.

The C-visible `_Static_assert(offsetof(struct rts_task, saved_stack_pointer) == 0)` verifies the only assembly-facing field contract. The build should generate or compiler-emit any later nonzero offsets; handwritten duplicate numeric offsets are prohibited.

## 3. Handles and validity

The public forward declaration and private definition use the same tag, `struct rts_task`; no cast is required. Equality and null comparison are valid. Applications cannot dereference an incomplete type. No public Version 1 function accepts an existing task handle, so forged-handle handling is not part of a public operation.

Internal validation checks pointer range and exact element alignment against the application pool before reading slot fields. Internal-only code may separately recognize `&idle_task_storage`. After range validation, assertion builds require `slot_state == ALLOCATED` and the validation magic. Pointer relational comparison against unrelated objects is not used; range checks use a representation-safe helper designed for the selected C implementation, or a bounded equality scan in assertion builds. A null handle means no task and is never a valid task object.

## 4. Task state and wait model

`DORMANT` means an allocated, initialized task not currently eligible for selection. It is used during creation before ready insertion and for the private idle task before scheduler initialization completes. Raw caller memory no longer exists; a `FREE` pool slot is not a task and therefore has no meaningful task state.

```text
FREE slot
   | allocate and initialize
   v
DORMANT --ready insertion--> READY --first/dispatch commit--> RUNNING
                              ^  ^                         |   |
                              |  +----preempt/yield--------+   |
                              |                                |
                              +------delay expires--- BLOCKED <-+
                                                   delay call
```

| From → To | Owner | Preconditions | Membership after transition |
|---|---|---|---|
| DORMANT → READY | Scheduler during creation/init | Valid allocated object; wait NONE; both nodes unlinked | Ready linked, delay unlinked |
| READY → RUNNING | Scheduler switch commit | Task is highest selected ready task | Ready linked; delay unlinked |
| RUNNING → READY | Scheduler | Preemption or peer rotation planned | Ready remains linked; queue order changes only through ready owner |
| RUNNING → BLOCKED | Scheduler | Nonzero delay; not idle | Ready unlinked; delay linked; wait DELAY |
| BLOCKED → READY | Scheduler on expiry | Deadline reached | Delay unlinked; ready linked; wait NONE |

The running task remains in its ready queue. `RUNNING` identifies which ready member executes; ready membership therefore represents all runnable tasks. This makes preemption preserve FIFO position, yield/quantum a single queue rotation, and highest selection constant-time. Exactly one task is `RUNNING` after startup, and it equals `current_task`.

The wait record contains only reason and wake tick. No wake-result field is needed because delay cannot be cancelled or complete with multiple outcomes. No object pointer or callback is reserved. `BLOCKED` implies `WAIT_DELAY`, delay-linked, ready-unlinked; every other state implies `WAIT_NONE`, delay-unlinked. Wake tick is meaningful only for `WAIT_DELAY`.

## 5. Kernel-global state and mutation ownership

One private static `rts_kernel_state_t` in the scheduler implementation owns the task-pool object, idle object, lifecycle, current/idle pointers, ready set, delay queue, tick, and switch plan. The pool object owns its slots, committed count, and free-search hint. Queue modules mutate only the contents of their owned structures when directed by the scheduler.

- Scheduler: lifecycle, task states/waits, current tick, current/idle pointers, switch plan, and quantum values.
- Task pool: application slot ownership metadata, committed count, and next-free hint.
- Task initialization contract: immutable task attributes and initial object representation during an unpublished transaction.
- Ready queue: ready node links, per-priority lists, ready bitmap.
- Delay queue: delay node links and ordered delayed list.
- Port: saved stack-pointer values and architecture execution state.

There is no scheduler lock or kernel critical nesting counter. The port owns debug tracking needed to validate critical token nesting.

## 6. Lifecycle

Static zero state is valid `RESET`. First `rts_init()` enters a critical section, initializes the port and queues, resets all slots, constructs the separate idle TCB and stack, inserts idle at priority zero, and changes lifecycle last to `INITIALIZED`. Recoverable port initialization failure leaves/re-establishes RESET and returns a public error. Repeated init returns `ALREADY_INITIALIZED`; init while running returns `ALREADY_STARTED`.

Creation before init or after start returns `INVALID_STATE`. `rts_start()` is legal with idle alone. It selects the highest ready task, prepares the first-task state, changes lifecycle to RUNNING immediately before architecture transfer, and calls the port start operation. Success never returns. A reported pre-transfer port failure restores INITIALIZED when safe and returns its status. An unexpected return after execution transfer is an unconditional fatal invariant.

## 7. Task-creation transaction

Public validation precedes mutation: nonnull config/output, non-ISR context, lifecycle, entry, priority, stack pointer, public alignment, byte size, and port constraints. `*out_handle` is set to null before allocation.

```text
validate public arguments
enter critical section
recheck INITIALIZED and capacity
reserve FREE pool slot (bounded scan), changing it to RESERVED
initialize candidate as unpublished DORMANT
initialize private scalar fields and both unlinked nodes
call port_stack_initialize(stack, size, entry, argument)
if port result fails:
    reset candidate task fields, then roll RESERVED back to FREE
    exit critical; return mapped status
store returned initial saved SP
insert candidate at ready tail through ready owner
if any internal invariant fails:
    remove if linked; reset candidate fields; roll RESERVED back to FREE; fatal or return before publication
set state READY
commit RESERVED slot to ALLOCATED (increments pool allocation count)
set validation magic last
publish exact candidate pointer to *out_handle
exit critical; return OK
```

Stack initialization is allowed to write the supplied stack before reporting failure; such bytes have no externally promised contents. Failure must not make them a valid runnable context. No handle, count, linked node, or valid magic is published until all fallible work succeeds.

## 8. Queue contracts and selection

The intrusive list is a nullable-head/tail mechanism, not a scheduler component. Empty is `{head=NULL,tail=NULL,count=0}` and unlinked linkage is `{previous=NULL,next=NULL,owner=NULL}`. The opaque `object` backlink is not part of linkage and is owned by the embedding queue; the list only clears it during node initialization. All primitive insert/remove operations are constant time and contain no task, priority, tick, state, port, or scheduling knowledge.

The ready set is exactly one FIFO list per configured priority plus a `uint32_t` bitmap of nonempty priorities. Insert is tail insertion, removal is constant time, highest selection scans a compile-time-bounded number of bitmap words and selects the most-significant ready bit, and rotation moves the head at one priority to its tail. `rts_ready_contains` validates that the task's ready-node owner is the expected priority list. The scheduler, not the ready module, changes states or requests switches. Idle is the sole permitted priority-zero member and guarantees nonempty selection after initialization. Numerically highest priority wins.

The delay queue is ordered by modular wake time within the half-range contract. Conceptually, deadline `d` is due at `now` when the signed 32-bit interpretation of the modulo-2^32 difference `(now - d)` is nonnegative; insertion uses the same half-range ordering. No accepted delay exceeds `INT32_MAX`, so ordering against the current scheduling horizon is unambiguous. Equal wake ticks preserve insertion order. The scheduler repeatedly peeks then removes expired heads; this is the extraction contract for all due tasks. A tick performs no arbitrary full-list scan: simultaneous-expiry work is proportional only to the number expiring at that tick and bounded by `RTS_MAX_TASKS`. `rts_delay_contains` validates owner membership. The delay module changes links only, never state or readiness.

Selection rules are: higher ready priority preempts; equal-priority creation cannot happen while running; yield rotates only the current priority when a peer exists; quantum expiry rotates only when a peer exists; blocking removes current from ready before selecting; wakeup preempts only if its priority is greater than current; idle wins only when no application task is ready.

## 9. Port and initial-stack contracts

The port exposes read-only minimum-size and size-granularity queries so portable descriptor validation can reject an unusable stack without encoding an architecture frame size. `rts_port_stack_initialize()` returns `{status, saved_stack_pointer}` rather than using null as an overloaded error. It independently validates the same port constraints, constructs the architecture-private initial frame, and returns the saved SP representation expected by context restore. Failures include invalid alignment/size, arithmetic overflow in bounds/frame placement, or unsupported architecture configuration.

| Operation | Caller / phase | Context | Failure | Bound and side effects | Host port |
|---|---|---|---|---|---|
| `rts_port_initialize` | `rts_init`, RESET | Startup | Status | Bounded; architecture scheduler setup only | Required |
| `rts_port_task_stack_minimum_size_bytes` | descriptor validation | Any | None; nonzero contract | Constant, read-only | Required |
| `rts_port_task_stack_size_granularity_bytes` | descriptor validation | Any | None; nonzero contract | Constant, read-only | Required |
| `rts_port_stack_initialize` | creation/idle init | Startup, inside kernel critical region | Result status | Bounded by fixed frame size; writes supplied stack | Required |
| `rts_port_critical_enter` | portable kernel | Task, startup, or eligible ISR | No normal failure | Constant time; returns prior mask token | Required |
| `rts_port_critical_exit` | matching caller | Same logical context | Misuse asserts/fatal | Constant time; restores exact prior state | Required |
| `rts_port_is_in_isr` | public validation/assertions | Any | No | Constant, no mutation | Required |
| `rts_port_request_context_switch` | scheduler | Critical region/task or tick ISR | No | Constant; coalescing request | Required |
| `rts_port_start_first_task` | `rts_start`, INITIALIZED→RUNNING | Startup | Status before transfer only | On success does not return | Required/emulated |

Timer initialization is target-owned and absent from this interface.

## 10. Context-switch exchange

Model C is selected: the scheduler prepares a `switch_plan {from,to,pending}` before requesting the port switch. Model A was rejected because exported loose globals weaken ownership; Model B was rejected because policy selection inside PendSV couples exception code to C ABI and lengthens the exception path.

While holding a critical section, scheduler selection writes `from = current_task`, replaces `to` with the latest selected task, and sets pending before requesting the switch. A later scheduling event before PendSV recomputes `to`; higher-priority wakeups therefore supersede earlier candidates. If recomputation selects `current_task`, the scheduler clears pending; an already-pended exception then performs a no-op. Repeated requests otherwise coalesce.

The PendSV C wrapper masks eligible kernel interrupts and calls `rts_scheduler_switch_acquire(&snapshot)`. False means no valid work. True returns a stable local `{from,to,generation}` snapshot with distinct, valid allocated tasks. Assembly saves `snapshot.from->saved_stack_pointer` and restores `snapshot.to->saved_stack_pointer`; afterward the wrapper calls `rts_scheduler_switch_complete(&snapshot)`. Completion verifies that the active plan generation and pointers still match, changes the outgoing RUNNING task to READY, changes `to` to RUNNING, assigns `current_task = to`, and clears the active plan. Events observed while the snapshot is active set a deferred reselection flag without mutating the snapshot. No eligible event can modify the plan during acquire/complete because the port retains the required mask. The port performs mechanics but never selects. First-task startup uses no outgoing task and is handled only by `rts_port_start_first_task()`.

## 11. Idle task

Idle TCB storage and its statically declared aligned stack are private kernel objects; idle stack size is selected by required `RTS_IDLE_STACK_SIZE_BYTES` configuration and validated like other stacks. Initialization uses the same task/port contracts, priority zero, and a private fixed entry loop. It is inserted into the ordinary ready queue. Applications cannot create priority-zero tasks or obtain the idle handle. Idle calls no public blocking/yield API. Version 1 has no weak idle hook and no low-power behavior. It cannot execute before `rts_start()` even though it is ready after init.

## 12. Tick entry

The target timer ISR calls `rts_kernel_tick_isr()`. The entry requires a configured kernel-callable ISR priority and RUNNING lifecycle. It enters the kernel critical contract, increments the 32-bit tick modulo 2^32, expires every due delay head, changes each expired task to READY through scheduler orchestration, and evaluates higher-priority preemption. It then accounts the current non-idle task's quantum; expiry rotates only an equal-priority peer and reloads accounting.

The tick entry does not return a switch boolean. The portable scheduler prepares/coalesces the switch plan and calls `rts_port_request_context_switch()` internally, preserving the rule that the target does not make scheduling decisions. Delays are limited to `RTS_DELAY_MAX`, and signed-half-range-equivalent modular comparison is implemented without signed overflow. Simultaneous expirations are all processed before final selection, producing at most one pending switch request.

## 13. Critical sections

`rts_critical_token_t` is an opaque-to-kernel `uintptr_t` value containing the previous port mask state. Enter has constant-time postconditions: eligible kernel interrupts cannot observe protected state and the returned token exactly represents the prior state. Every enter has one lexical matching exit in LIFO order. Exit has constant-time postconditions: all protected mutations are complete and the exact saved state is restored. Nested entries are valid because each token restores its own prior state; the port, not the kernel, tracks nesting only for assertion validation. The host port emulates the same prior-state and LIFO rules. The contract may be used during startup, task context, and the tick ISR only when that ISR is in the configured kernel-callable class.

No blocking call, queue-visible partial transaction, first-task launch, or context-switch commit occurs while an ordinary C critical section remains open. A switch may be requested inside but takes effect only after restoration. Tokens may not cross functions as stored global state, be reused, or be exited out of order. PRIMASK versus BASEPRI is entirely port-private.

## 14. Validation and assertions

- **Compile time:** tick width, 8-bit bytes, priority range, power-of-two stack alignment, saved-SP offset zero, pool capacity, idle stack constraints, and port configuration.
- **Public validation/status:** null arguments, ISR context, lifecycle, entry, priority, stack address/size, capacity, and stack-initialization failure.
- **Debug assertions:** magic, pool membership, allocated slot, legal state transition, node owner/unlinked state, wait/state correspondence, stack bounds, current task consistency, token nesting, and switch-plan consistency.
- **Unconditional fatal invariants:** corrupted ready/delay links detected in release, no ready task despite idle after initialization, context-switch target invalid, unexpected return after successful first-task transfer, and impossible port/kernel context mismatch where continuing could execute an invalid context.

Recoverable application input never becomes a fatal assertion before its documented status is returned.

## 15. Internal headers and include boundaries

```text
rts_types.h
   ├── config_internal.h
   ├── port.h
   └── intrusive_list.h
          └── task_internal.h
                 ├── ready_queue.h
                 └── delay_queue.h
                         └── scheduler_internal.h

assert_internal.h -> rts_types.h
```

- `scheduler_internal.h`: scheduler-owned lifecycle, kernel state, switch plan, tick/prepare/get/commit contracts; consumed by scheduler, port mechanics, target tick binding, and scheduler white-box tests; no public APIs.
- `task_internal.h`: real `struct rts_task`, state/wait/slot enums, TCB initialization and validation declarations; consumed by portable kernel, queues, port, and task white-box tests; no queue algorithms or kernel globals.
- `ready_queue.h`: ready-set representation and link-only operations; consumed by scheduler and ready white-box tests; no state transitions or switch requests.
- `delay_queue.h`: delay representation, ordered link operations, expiry peek, and modular-time predicate; consumed by scheduler and delay tests; no timer hardware or state transitions.
- `intrusive_list.h`: generic node/list representation and constant-time primitive declarations; consumed only by private structures and white-box tests; no TCB knowledge.
- `config_internal.h`: portable derived constants and static checks; consumed by private kernel units/tests; no target configuration.
- `port.h`: minimum portable-to-port contract and result/token types; consumed by scheduler/task creation and selected port; no queues or target timer.
- `assert_internal.h`: assertion/fatal macros and failure hook declaration; consumed by private modules; no diagnostic API.

All eight are PRIVATE CMake includes. Dedicated white-box tests may include their target header through a test-only private include path. Public/application targets never receive `kernel/` transitively.

## 16. Sequence diagrams

```text
Successful create:
API -> Scheduler: validate/lifecycle, enter critical, reserve FREE slot
Scheduler -> Task: initialize DORMANT typed object
Task -> Port: construct initial stack; return status + SP
Scheduler -> Ready: insert tail
Scheduler: state READY, count++, magic, publish handle, exit critical

Startup:
API -> Scheduler: validate INITIALIZED, choose Ready.highest
Scheduler: prepare first task RUNNING and lifecycle RUNNING
Scheduler -> Port: start_first_task
Port: restore prepared context; success never returns

Yield with peer:
API -> Scheduler: validate RUNNING/task context; enter critical
Scheduler -> Ready: has_peer, rotate current priority, peek highest
Scheduler: reset slice, prepare switch
Scheduler -> Port: request switch; exit critical

Delay:
API -> Scheduler: validate; enter critical
Scheduler -> Ready: remove current
Scheduler: state BLOCKED, wait DELAY, compute wake
Scheduler -> Delay: ordered insert
Scheduler -> Ready: peek highest; prepare switch
Scheduler -> Port: request; exit critical

Higher-priority expiry:
Target timer -> Scheduler tick ISR: tick++
Scheduler -> Delay: peek/remove all expired
Scheduler: wait NONE, state READY
Scheduler -> Ready: insert each; select highest
Scheduler -> Port: request one coalesced switch

PendSV switch:
Port -> Scheduler: switch_acquire(snapshot); false means no-op
Port: save snapshot.from SP; restore snapshot.to SP
Port -> Scheduler: switch_complete(snapshot)
Scheduler: states/current/pending updated
Port: exception return to selected task

Tick without change:
Target timer -> Scheduler: tick++, no expired head
Scheduler: decrement/reload slice as applicable, selection unchanged
Scheduler: exit without port switch request
```

## 17. Risk review

| Risk / failure | Prevention rule | Required test | Blocks implementation? |
|---|---|---|---|
| Pool RAM growth | Report `sizeof(TCB)*RTS_MAX_TASKS + idle`; review optional fields | Map-file budget test per config | No |
| TCB alignment | Real typed arrays, no overlays | Compile/static alignment tests host/ARM | No |
| Assembly offset drift | SP at offset zero plus static/generated verification | Build disassembly/offset test | No |
| Running membership error | Running always ready-linked invariant | State/queue transition unit matrix | No |
| Lost switch request | Critical switch plan and coalescing | Inject multiple events before PendSV | No |
| Nested critical misuse | Token LIFO and port debug nesting | Nested restore/misorder tests | No |
| Tick overflow | Half-range modular predicate | Boundary tests around `UINT32_MAX` | No |
| Simultaneous expiry | Drain all due heads before one selection | Same-wake multi-priority test | No |
| First-task special path | Dedicated start contract, null outgoing context | Host emulation and target smoke test | No |
| Host/hardware divergence | Same port contract and event-level conformance suite | Run shared contract tests on both | No |
| Time-slicing layout change | Slice field always present | `sizeof/offsetof` builds on/off | No |
| Configuration mismatch | One config target; header checks; build identity | Negative multi-config/build tests | No |

## 18. Implementation-review invariants

1. Every application TCB is an actual element of the private typed pool.
2. Every slot is exactly FREE, RESERVED, or ALLOCATED; only the pool manager changes ownership metadata in Version 1.
3. Idle is a separate typed object and never consumes or returns an application slot.
4. Only the scheduler changes task state, wait reason, current task, or switch plan.
5. Only the ready module mutates ready links/bitmap; only delay mutates delay links.
6. DORMANT is unlinked; READY and RUNNING are ready-linked; BLOCKED is delay-linked only.
7. RUNNING remains in the ready queue and equals `current_task` after startup.
8. WAIT_DELAY is equivalent to BLOCKED plus delay membership; all others have WAIT_NONE.
9. Idle is permanently ready after initialization and is never blocked or rotated by a public API.
10. The port saves/restores context but never selects a task.
11. Saved SP remains at verified TCB offset zero.
12. A switch plan is written and consumed under the critical contract; requests coalesce without loss.
13. No blocking or actual execution transfer occurs inside an open C critical section.
14. Delay arithmetic accepts at most half the tick range and remains correct across wrap.
15. A published handle is null or points directly to a stable ALLOCATED pool element.
16. Failed creation publishes no handle, link, count, magic, or allocated slot.
17. Application input errors return status before becoming internal corruption assertions.
18. No private header is transitively exposed through `rts_api`.

## Approved Version 1 Internal Kernel Baseline

The final private TCB is the exact `struct rts_task` in `kernel/task_internal.h`: saved SP at verified offset zero; stack bounds; entry and argument; separate ready/delay nodes; wait reason and wake tick; unconditional slice counter; priority, task state, slot state; and assertion-only magic. Handles point directly to these objects.

Task states are DORMANT, READY, RUNNING, and BLOCKED. Wait metadata contains only NONE/DELAY and wake tick. Pool ownership is FREE/RESERVED/ALLOCATED; committed allocation is permanent in Version 1.

The final kernel state is the exact `rts_kernel_state_t` in `scheduler_internal.h`, including the application-pool object, separate idle object and aligned idle stack, lifecycle, current/idle pointers, ready set, delay queue, tick, and switch plan. The pool object contains its own allocation count and free hint.

The running task remains in the ready queue. The scheduler prepares and coalesces a from/to switch plan before requesting the port; the masked PendSV wrapper acquires a stable snapshot, performs mechanics, and commits that exact snapshot without policy selection.

The final port contract is `initialize`, read-only task-stack minimum/granularity queries, `stack_initialize`, `critical_enter/exit`, `is_in_isr`, `request_context_switch`, and `start_first_task`. Stack initialization returns status plus saved SP. The target timer is not part of the port.

Task creation validates first, changes a FREE typed slot to RESERVED, initializes it as unpublished DORMANT with null saved SP, constructs its stack through the port, links it ready, transitions it to READY, commits the slot to ALLOCATED, sets valid magic, then publishes the direct pointer. Any failure unlinks and resets task fields as necessary, then rolls the RESERVED slot back to FREE before return.

Idle storage and stack are private and separate, priority zero, ordinary-ready-queue resident, fixed-loop, and unavailable publicly. Tick entry increments modular time, drains simultaneous expirations, accounts optional slicing, performs one final selection, prepares a switch plan if needed, and requests the port internally.

Critical sections use opaque prior-mask tokens with LIFO nesting owned/validated by the port. They are private; switching is requested but not executed while open.

The final internal header list is `scheduler_internal.h`, `task_internal.h`, `ready_queue.h`, `delay_queue.h`, `intrusive_list.h`, `config_internal.h`, `port.h`, and `assert_internal.h`. The 18 invariants in section 18 are mandatory and close the Version 1 internal architecture.
