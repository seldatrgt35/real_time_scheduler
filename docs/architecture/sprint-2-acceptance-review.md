# Sprint 2 Architecture Acceptance Review

**Review scope:** Version 1 public API, private kernel contracts, ownership rules, C11 type model, configuration, and Sprint 3 implementation readiness.  
**Review date:** 2026-07-14

## 1. Reviewed artifacts

- `docs/architecture/sprint-0-baseline.md`
- `docs/architecture/version-1-public-api.md`
- `docs/architecture/sprint-2-storage-blocker.md`
- `docs/architecture/sprint-2-internal-kernel.md`
- `include/rts/rts.h`
- `include/rts/rts_task.h`
- `include/rts/rts_types.h`
- all eight headers under `kernel/`
- `tests/config/rts_config.h`

Both host Clang and Clang targeting `arm-none-eabi`/Cortex-M4 accepted a translation unit containing every public and private header under C11 with `-Wall -Wextra -Werror`.

## 2. Findings and corrections

| Finding | Severity | Correction |
|---|---|---|
| Sprint 0 retained two obsolete caller-owned TCB-storage statements. | Blocking consistency | Replaced them with typed private-pool ownership and caller-owned stack wording. |
| Tick-rate configuration was named `RTS_TICK_HZ`, while the acceptance contract names `RTS_TICK_RATE_HZ`. | Blocking consistency | Renamed the option in public validation, documentation, and test configuration. No alias/default remains. |
| `RTS_STATUS_INTERNAL_ERROR` could incorrectly turn kernel corruption into a recoverable public result. | Blocking semantics | Replaced it with `RTS_STATUS_PORT_ERROR`, limited to recoverable port initialization or pre-transfer startup failure. Corruption is fatal. |
| Assertion-only node ownership could not identify a linked singleton in release builds. | Blocking queue invariant | Made `rts_list_node_t.owner` unconditional and typed as `rts_list_t *`; added `rts_list_node_is_linked`. |
| Ready/delay contracts lacked explicit membership queries for assertions and white-box tests. | Non-blocking but required for reviewability | Added `rts_ready_contains` and `rts_delay_contains`; neither mutates state. |
| Kernel state owned an idle TCB but did not structurally own its configured stack. | Blocking ownership | Added a 16-byte-aligned `idle_stack[RTS_IDLE_STACK_SIZE_BYTES]` to private kernel state. |
| The old switch-plan getter exposed live plan storage and completion accepted only a task pointer. | Blocking race/ownership | Replaced it with masked `switch_acquire(snapshot)` and generation-checked `switch_complete(snapshot)` contracts. |
| Private state enums had implementation-selected widths. | Non-blocking determinism | Made task state, wait reason, slot state, and lifecycle fixed-width `uint8_t` aliases with named constants. |
| Intrusive-list and modular-delay pre/postconditions were implicit. | Blocking contract clarity | Added canonical node/list representation, owner rules, bounded operations, and conceptual half-range comparison to the internal baseline. |

No unrelated-object cast, public TCB exposure, packing directive, flexible array, zero-length array, or compiler aliasing exception remains.

## 3. Final C type and object-lifetime review

The public declaration `struct rts_task;` and private definition `struct rts_task { ... };` are the same C tag. `rts_task_handle_t` is `struct rts_task *` and points directly to an actual element of the typed private pool. Applications may copy and compare handles but cannot dereference the incomplete type. The idle pointer has the same private type but is never published.

Private headers reuse public `rts_tick_t`, `rts_priority_t`, `rts_status_t`, and `rts_task_entry_t`. No scalar is redefined. All arrays have positive compile-time bounds after validation. `_Alignas(16)` is standard C11, no packing is used, and the only assembly-visible contract is verified with:

```c
_Static_assert(offsetof(struct rts_task, saved_stack_pointer) == 0,
               "saved stack pointer must remain at TCB offset zero");
```

There is no public/private storage overlay, so size/alignment assertions between unrelated public and private types are neither required nor permitted.

## 4. TCB layout acceptance

Every field has a current Version 1 purpose. The two intrusive nodes have independent unconditional owners. `slice_remaining` is unconditional, so enabling time slicing does not change TCB layout. Only assertion enablement adds `validation_magic`; it never changes saved-SP offset zero. Idle and application tasks use the identical type.

Measured/review estimates with the current declaration:

| Build model | Assertions disabled | Assertions enabled | Alignment |
|---|---:|---:|---:|
| 32-bit ARM EABI/Cortex-M4 | approximately 68 bytes | measured 72 bytes | 4 bytes |
| Typical 64-bit host | approximately 120 bytes | measured 128 bytes | 8 bytes |

The ARM and host assertion-enabled values were obtained from compiler record-layout dumps. They are not ABI constants. Sprint 3 must retain compile-time offset checks and add map-file RAM-budget reporting.

No EDF, synchronization, deletion, SMP, MPU, trace, statistics, name, deadline, or period field exists.

## 5. Pool and handle invariants

Sprint 4.1 supersedes the original two-state transaction encoding with three explicit slot states.

- **FREE:** `slot_state == FREE`, magic zero when present, both nodes canonical-unlinked, no published handle, not counted, and all other bytes treated as non-task/reset state.
- **Reserved/unpublished:** `slot_state == RESERVED`, magic zero, state DORMANT after task-object initialization, both nodes initially unlinked, not counted, and no handle published. The creation transaction will occur under one critical section while lifecycle is INITIALIZED.
- **Allocated/published:** `slot_state == ALLOCATED`, valid magic when enabled, state READY initially, ready-linked, counted exactly once, and its direct pointer has been stored in `out_handle`.

`RESERVED` is ownership metadata rather than a runnable task state. Every later creation failure must unlink any node, clear/reset initialized task data as appropriate, roll the slot back to FREE, leave the count unchanged, and leave `*out_handle == NULL`.

As refined by Sprint 4.1, the first-free search starts at the pool-owned hint and examines no more than `RTS_MAX_TASKS`; rollback may retarget the hint to the returned slot. Committed slots are never freed, so deletion and generation numbers are unnecessary. Capacity exhaustion is reported without mutation. Pool `allocated_count` means committed application tasks only; reserved slots and idle are excluded.

## 6. State and membership matrix

| State | Ready node | Delay node | Wait | May equal current | Selectable | Legal transitions |
|---|---|---|---|---|---|---|
| DORMANT | Unlinked | Unlinked | NONE | No | No | to READY during committed initialization |
| READY | Linked | Unlinked | NONE | No | Yes | to RUNNING on switch commit |
| RUNNING | Linked | Unlinked | NONE | Yes, exactly | Already executing | to READY on switch commit away; to BLOCKED on nonzero delay |
| BLOCKED | Unlinked | Linked | DELAY | No | No | to READY on expiry |

A FREE slot has no meaningful task state. The scheduler alone mutates states/wait metadata. Ready and delay modules alone mutate their respective links. Task initialization establishes unpublished DORMANT fields but performs no runtime transition. Running remains in its ready queue. Idle is permanently ready-linked after initialization, becomes RUNNING only when selected, and is never blocked or rotated through public APIs.

## 7. Final queue representations

### Intrusive list

The list uses nullable head/tail, not a sentinel. Empty is `(NULL,NULL,0)` and linkage is unlinked when `(previous=NULL,next=NULL,owner=NULL)`. The embedding queue owns an opaque object backlink used for strict C11 object recovery; the list does not interpret it. Insert-before, push-back, and removal are constant time. The layer knows no tasks, priorities, states, wake ticks, port, or scheduling decisions.

### Ready set

The selected representation is `RTS_PRIORITY_COUNT` FIFO lists plus a `uint32_t` nonempty bitmap. Tail insertion provides equal-priority FIFO. Highest numeric set priority wins. Highest selection examines at most `ceil(RTS_PRIORITY_COUNT/32)` words—at most eight under the validated Version 1 limit. Rotation moves one priority-list head to its tail and is used only when the scheduler has established a peer. Priority zero contains idle only.

### Delay queue

The selected representation is one wake-ordered intrusive list. Conceptually, deadline `d` is due when the signed interpretation of modulo difference `now-d` is nonnegative. Accepted relative delays are at most `INT32_MAX`, preserving unambiguous half-range ordering. Equal deadlines retain insertion order. The tick path examines/removes only due heads, so its work is proportional to simultaneous expirations rather than all blocked tasks.

## 8. Kernel state and context-switch exchange

`rts_kernel_state_t` contains the typed application-pool object, separate idle TCB and stack, lifecycle, current/idle pointers, ready set, delay queue, current tick, and one switch plan. The embedded pool object owns its committed count and free hint. No externally writable kernel global is declared in a header.

The scheduler selects and prepares `from=current_task`, the latest `to`, and pending state under a critical section. New events before PendSV replace `to`; if selection returns to current, pending is cleared. Multiple hardware requests may remain pended, but a stale exception acquires no plan and exits without switching.

PendSV's C wrapper retains the kernel interrupt mask while acquiring a local plan snapshot, saving outgoing SP, restoring incoming SP, and committing that exact snapshot. Commit alone changes `current_task`, RUNNING/READY states, and pending state. `from == to` is never acquired as a valid switch. First-task startup uses the dedicated port operation and no outgoing TCB. The port never selects.

## 9. Critical-section acceptance

The private token preserves the exact prior interrupt state. Enters/exits are constant time, lexically paired, LIFO-nested, and valid only in startup, task context, or configured kernel-callable ISR context. The port owns assertion-only nesting metadata; the host port must emulate the same token and restoration behavior. Queue/state mutation is complete before exit. A switch request may be recorded while critical, but execution transfer is deferred until masking is restored. No public API exposes the mechanism, and portable names mention neither PRIMASK nor BASEPRI.

## 10. Actual include graph

```text
rts_config.h
    -> rts/rts_types.h
         -> rts/rts.h
         -> rts/rts_task.h
         -> kernel/port.h
         -> kernel/config_internal.h
         -> kernel/assert_internal.h
         -> kernel/intrusive_list.h
              -> kernel/task_internal.h
                   -> kernel/ready_queue.h
                   -> kernel/delay_queue.h
                        \-> kernel/scheduler_internal.h
```

`scheduler_internal.h` includes both ready and delay headers. No cycle exists. `port.h` depends only on public scalar/API types, not queues or the TCB. `intrusive_list.h` does not depend on task internals. Target/CMSIS/NXP headers appear nowhere in public or portable private headers. Every private header passed a direct aggregate C11 inclusion check.

## 11. Configuration contract

| Option | Ownership and validation | Affected module | Layout effect | Behavior effect |
|---|---|---|---|---|
| `RTS_MAX_TASKS` | Application-selected; required, >=1 | Kernel state/pool | Pool and ready/delay maximum RAM | Capacity and bounded startup/expiry work |
| `RTS_PRIORITY_COUNT` | Application-selected; required, 2..256 | Public validation, ready set | Priority-list and bitmap arrays | Valid priority range/selection bound |
| `RTS_TICK_RATE_HZ` | Application-selected; required, >0 | Target timer contract/application conversion | None | Physical duration of one tick |
| `RTS_ENABLE_TIME_SLICING` | Application-selected; exactly 0 or 1 | Scheduler tick | None | Enables peer quantum rotation |
| `RTS_TIME_SLICE_TICKS` | Application-selected; required; >0 when slicing enabled | Scheduler tick | None | Quantum length |
| `RTS_IDLE_STACK_SIZE_BYTES` | Application integration-selected; required, >0 and port-minimum compliant | Kernel state/port init | Kernel-state RAM | Idle stack capacity |
| `RTS_ENABLE_ASSERTIONS` | Application-selected; exactly 0 or 1 | All private modules | Adds TCB magic and list/check code | Adds validation/fatal checks; never scheduling decisions |
| `RTS_TASK_STACK_ALIGNMENT` | Library-fixed at 16, power-of-two checked | Public declaration/port validation | Application stack alignment | Valid stack acceptance only |

There are no hidden defaults. Exactly one CMake configuration include directory must supply `rts_config.h` to every target in one final image. Host and S32K148 images may select different configurations because they are different images; one image cannot link objects compiled against different selections. Sprint 3 must add a build-level configuration identity guard in addition to header range checks.

## 12. Status and lifecycle review

Public statuses cover null/invalid arguments, invalid context, invalid lifecycle, invalid task configuration, priority, stack, capacity, repeated init/start, and recoverable port failure. Assertions/fatal corruption do not return `PORT_ERROR`.

| API | Valid state/context | Relevant failures |
|---|---|---|
| `rts_init` | RESET, startup non-ISR | INVALID_CONTEXT, ALREADY_INITIALIZED/STARTED, PORT_ERROR |
| `rts_start` | INITIALIZED, startup non-ISR | INVALID_CONTEXT, INVALID_STATE/ALREADY_STARTED, PORT_ERROR before transfer |
| `rts_task_create` | INITIALIZED, startup non-ISR | argument/config/priority/stack/capacity/state/context |
| `rts_task_yield` | RUNNING non-idle task | INVALID_STATE or INVALID_CONTEXT |
| `rts_task_delay` | RUNNING non-idle task | INVALID_ARGUMENT for >half-range, INVALID_STATE/CONTEXT; zero delegates to yield |

Successful start does not return. Starting with idle only is valid.

## 13. Required Sprint 3 tests

- **Task/pool:** static-zero FREE representation, monotonic allocation, exact capacity, exhaustion without mutation, reserved-unpublished invariants, rollback at every fallible step, count/hint behavior, stable direct handles, forged/null/internal-idle validation.
- **Intrusive list:** empty/init, singleton ownership, front/middle/tail removal, insertion preconditions, double-link rejection, wrong-owner rejection, canonical unlink restoration, count/neighbor consistency.
- **Ready set:** every priority, highest numeric selection, bitmap word boundaries, idle-only selection, equal-priority FIFO, removal/bitmap clearing, peer detection, rotation, running-member containment.
- **Delay queue:** ordered and reverse insertion, equal-deadline FIFO, removal, no-expiry head check, simultaneous expiry, `UINT32_MAX` wrap boundaries, zero/maximum accepted horizon.
- **Lifecycle/API:** every call in RESET/INITIALIZED/RUNNING, repeated init/start, no-application start, creation after start rejection, ISR misuse, null descriptors/output, invalid entry/priority/stack.
- **Scheduler decisions:** higher-priority wake preemption, equal-priority wake without immediate preemption, yield with/without peer, lower priority never selected solely by yield, zero delay equivalence, nonzero block/wake, idle fallback.
- **Time slicing:** disabled behavior, enabled decrement/reload, no-peer expiry, peer rotation, preservation across higher-priority preemption, yield reset.
- **Switch exchange:** initial plan, repeated request coalescing, candidate replacement, cancellation to current, stale PendSV no-op, acquire/commit snapshot match, higher-priority ISR before PendSV, current/state commit consistency.
- **Critical/host port:** prior-state restoration, nested LIFO, invalid token/order assertion, ISR-context reporting, deferred switch recording, first-task nonreturn emulation, saved-SP recording.
- **Layout/configuration:** saved-SP offset zero on host/ARM, measured TCB sizes with assertions on/off, idle alignment, negative tests for every invalid macro, and mixed-configuration link/build rejection.
- **Target smoke prerequisites:** SVC first launch, PSP task execution/MSP exception use, PendSV save/restore, tick delivery, and idle-only startup; implementation remains Sprint 3+ work.

## 14. Remaining non-blocking risks

- Assertion enablement changes private TCB size and pool RAM. This is explicit, map-tested, and does not move saved SP.
- Ordered delay insertion is O(number of blocked tasks). It occurs in task context and is bounded by `RTS_MAX_TASKS`; target WCET must be measured.
- Simultaneous tick expiry is O(number expiring). This is intentional and bounded; target interrupt-budget analysis remains required.
- The host port cannot reproduce exception tail-chaining or exact interrupt latency. Shared semantic tests plus target smoke tests are required.
- The free hint is only a bounded-search starting point; rollback may move it to a returned RESERVED slot, while committed slots remain permanent because Version 1 has no deletion.
- The switch wrapper/assembly offset-generation mechanism must be selected during port implementation, but handwritten duplicate offsets are already prohibited.

## 15. Acceptance criteria

| Criterion | Result |
|---|---|
| Cross-document storage, stack, capacity, lifecycle, priority, idle, delay, slicing, port, timer, critical, and queue ownership consistency | PASS |
| Strict C11 object/type/alignment model | PASS |
| Minimal current-purpose TCB with verified SP offset | PASS |
| Transactional bounded pool model without deletion | PASS |
| Single running-task/ready-membership model | PASS |
| Policy-free constant-time intrusive-list mechanism | PASS |
| Deterministic fixed-priority ready representation | PASS |
| Wrap-safe ordered delay contract and bounded tick work | PASS |
| Singular kernel-global ownership | PASS |
| Race-safe scheduler-selected context-switch exchange | PASS |
| Nested prior-state critical contract | PASS |
| Acyclic, self-contained private header graph | PASS |
| Explicit configuration with no hidden defaults | PASS |
| Public status/lifecycle/context coverage | PASS |
| Implementable host and target test strategy | PASS |

## Sprint 2 Acceptance Decision

ACCEPTED — Ready for Sprint 3 implementation
