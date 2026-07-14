# Sprint 4 Integration Review and Acceptance Gate

**Review scope:** Sprint 4.1 through Sprint 4.5 and their Sprint 2/3 dependencies  
**Decision:** Accepted after the focused corrections recorded below

## 1. Reviewed artifacts

The review covered the Version 1 public API and Sprint 0/2/3 architecture and acceptance baselines; every Sprint 4 implementation document; all public headers; all private kernel headers; the pool, validation, task-object, task-creation, kernel-state, intrusive-list, ready-queue, delay-queue, and host-port sources; CMake composition; and all focused unit tests.

No scheduler start, current-task selection, idle execution, yield/delay orchestration, tick processing, exception implementation, or target context switching was found in Sprint 4.

## 2. Findings and corrections

The integration was structurally sound, with three correctable omissions:

1. `rts_task_handle_is_application_task()` and `rts_task_object_is_valid()` were declared but not implemented. A private bounded equality scan now recognizes only actual application-pool elements before any object dereference. Idle, null, and forged pointers are rejected. Full validity additionally requires ALLOCATED/READY state, saved SP and stack metadata, valid application priority, NONE wait, unlinked delay node, ready-set membership, and assertion magic when configured.
2. Creation tests covered equal-priority FIFO but not mixed-priority visibility. A focused test now creates low, high, and middle priorities and verifies that highest-ready selection immediately sees the high-priority task while every task remains registered.
3. A single rollback was tested, but repeated failure leakage was not. Fault injection now repeats beyond `RTS_MAX_TASKS` attempts and proves count zero, slot zero FREE, hint zero, no ready member, null handles, and balanced critical depth after every attempt. The rollback test also verifies the complete canonical free TCB representation.

No public API, TCB layout, scheduling policy, allocation model, or queue representation changed.

## 3. Final task-creation sequence

The implemented order is:

1. Reject null `out_handle`.
2. Write null to a valid output location.
3. Query and reject ISR context; assertion builds also report misuse.
4. Validate descriptor and lifecycle without mutation.
5. Enter the selected-port critical section.
6. Recheck lifecycle is INITIALIZED.
7. Reserve one FREE application slot, producing RESERVED.
8. Initialize the reserved TCB as DORMANT with canonical nodes and null saved SP.
9. Ask the selected port to construct the initial stack.
10. Store the successful saved SP in the private TCB.
11. Insert through the ready-queue owner at the priority FIFO tail.
12. Change DORMANT to READY.
13. Commit RESERVED to ALLOCATED; the pool increments committed count.
14. Set assertion validity magic when enabled.
15. Publish the exact `struct rts_task *` through `out_handle`.
16. Restore the exact prior critical state.

No fallible public operation follows handle publication. No context-switch request or task selection occurs because creation is INITIALIZED-only.

## 4. Rollback matrix

| Failure point | Public result | Slot/queue effect | Critical/output postcondition |
|---|---|---|---|
| Null output pointer | INVALID_ARGUMENT | None | No critical entry; no writable output exists |
| ISR context | INVALID_CONTEXT | None | Output null; no critical entry |
| Null descriptor | INVALID_ARGUMENT | None | Output null; no critical entry |
| Lifecycle RESET/RUNNING during validation | INVALID_STATE | None | Output null; no critical entry |
| Null entry | INVALID_TASK_CONFIG | None | Output null; no critical entry |
| Invalid priority | INVALID_PRIORITY | None | Output null; no critical entry |
| Invalid stack | INVALID_STACK | None | Output null; no critical entry |
| Lifecycle changes before critical recheck | INVALID_STATE | None | Output null; prior critical state restored |
| No FREE slot | CAPACITY_EXHAUSTED | No reservation or queue change | Output null; prior critical state restored |
| Defensive TCB-initialization rejection | Propagated validation/state status | Candidate reset, RESERVED→FREE | Count unchanged; no links/magic/SP publication; critical restored |
| Recoverable port-stack failure | Port public status, normally PORT_ERROR | Candidate reset, RESERVED→FREE; application stack bytes are unspecified by contract | Count unchanged; no links/magic/SP publication; critical restored |
| Ready/commit invariant corruption | Fatal assertion contract | Defensive host-return path removes/reset/rolls back before refusing publication | Not a normal public status path; production fatal hook does not return |

Every recoverable failure leaves previously committed tasks untouched. Delay membership is never established. Rollback canonicalizes saved SP, bounds, entry/argument, nodes, wait, slice, priority, state, and magic before the pool returns ownership to FREE. Port failure may have modified the caller-owned stack, but no kernel state treats those bytes as a live context.

## 5. Pool and handle invariants

- FREE is non-owned and non-published. Production pool storage begins as static zero; failed initialized transactions use task reset before rollback, producing the same canonical free representation.
- RESERVED is one unpublished creation transaction. It is excluded from committed count and may be initialized or rolled back.
- ALLOCATED is committed permanently in Version 1. No deletion path returns it to FREE.
- Only the pool module changes slot ownership, count, and hint.
- `allocated_count` equals the number of ALLOCATED application elements after every public return.
- The idle object and idle stack are separate kernel-state members. Idle consumes no `RTS_MAX_TASKS` slot and is rejected by application-handle validation.
- Reservation examines at most `RTS_MAX_TASKS` slots beginning at a validated hint. Exhaustion is deterministic; rollback retargets the hint to the returned slot.
- The public handle points directly to an actual typed pool object. No overlay, unrelated-object reinterpretation, integer handle, or public layout exists.
- Publication occurs only after ready registration and commit. Version 1 deletion is absent, so a published handle is stable for system lifetime.

## 6. TCB success and free invariants

| Field/invariant | Committed application task | Canonical FREE slot |
|---|---|---|
| Slot state | ALLOCATED | FREE |
| Task state | READY | DORMANT neutral value; not meaningful while FREE |
| Saved SP | Nonnull, selected-port-produced | Null |
| Stack bounds | Nonnull valid exclusive range | Null/null |
| Entry/argument | Descriptor values | Null/null |
| Priority | `1..RTS_PRIORITY_COUNT-1` | Idle-priority neutral value |
| Wait/wake | NONE/zero | NONE/zero |
| Slice | Configured quantum | Zero |
| Ready node | Linked through ready owner | Canonical-unlinked |
| Delay node | Canonical-unlinked | Canonical-unlinked |
| Assertion magic | Valid magic | Zero |

The saved-stack-pointer offset-zero static assertion remains in force in every build. Assertion configuration changes only the trailing private magic field.

## 7. Ownership matrix

| Module | Exclusive Sprint 4 responsibility | Explicitly absent |
|---|---|---|
| Task pool | Slot states, bounded reservation, commit count, rollback, hint | TCB fields, queues, policy |
| Task validation | Public descriptor/lifecycle validation | Allocation, stack writes, TCB mutation |
| Task object | Reserved-object initialization and canonical reset | Pool commit, ready linking, frame construction |
| Selected port | Stack direction/frame, saved SP, critical/ISR mechanics | TCB/pool/queue/lifecycle mutation |
| Ready queue | Ready-node association, FIFO links, bitmap | Task state and switch decisions |
| Task creation | Transaction ordering, state transition, rollback, publication | Selection, current task, execution, runtime creation |
| Kernel state | One statically owned aggregate and private accessor | Implicit initialization |

Task creation performs no direct ready-node field writes. It calls only ready-queue operations for membership mutation. It never touches the delay queue.

## 8. Lifecycle and status matrix

| Condition | Status | Precedence |
|---|---|---:|
| Null output pointer | INVALID_ARGUMENT | 1 |
| ISR context with writable output | INVALID_CONTEXT | 2 |
| Null descriptor | INVALID_ARGUMENT | 3 |
| Lifecycle not INITIALIZED | INVALID_STATE | 4 |
| Null entry | INVALID_TASK_CONFIG | 5 |
| Invalid priority | INVALID_PRIORITY | 6 |
| Invalid stack | INVALID_STACK | 7 |
| Capacity exhausted | CAPACITY_EXHAUSTED | 8 |
| Recoverable port construction failure | Port's public `rts_status_t`, normally PORT_ERROR | 9 |
| Successful creation | OK | 10 |

RESET and RUNNING reject creation. INITIALIZED permits repeated calls through exact capacity. The API does not initialize the kernel implicitly and introduces no runtime-creation semantics. No private port enum crosses the public boundary.

## 9. Ready and host-stack integration

Ready insertion occurs only through `rts_ready_insert()`. Same-priority registration is tail FIFO; mixed-priority lookup immediately returns the numerically highest created priority. Priority zero is rejected. Creation records no switch request.

Descriptor validation and the host port agree on a 16-byte start/top alignment, a 16-byte size granularity, and the queried aligned minimum frame storage. The host port independently revalidates the region, detects end overflow with `uintptr_t`, writes only the top frame block, and returns its aligned start. Entry, argument, deterministic magic/version, zero reserved data, and return trap are decoded by white-box tests. Failure before writing leaves the byte region unchanged where promised; the generic port contract allows bytes to be unspecified after a later construction failure.

Host frame types and fault controls exist only below `ports/host/` and test include paths. Portable sources include only `kernel/port.h`. No CMSIS, NXP, or host frame type appears in a public or portable TCB contract.

## 10. C11 and configuration review

- Public task handles use the same incomplete/complete `struct rts_task` tag.
- Pool membership uses bounded pointer equality before dereference; it does not order unrelated pointers.
- Stack address and validation comparisons use `uintptr_t` with explicit overflow checks.
- Caller byte-stack storage is written through character accesses. Host frame fields are copied individually at standard `offsetof` positions, so declared byte-array type, strict aliasing, padding, and effective-type rules are preserved.
- Function pointers are compared with null and their object representations are byte-copied; entries are never called during creation.
- No packed type, alias extension, disabled strict aliasing, dynamic allocation, recursion, or compiler language extension is required.
- Fixed-width private state aliases preserve enum-storage assumptions.
- Assertion-disabled valid behavior follows the same mutations and results; optional magic does not control correctness.

The current CMake test image supplies exactly one `tests/config/rts_config.h` to every linked object. Assertion-off verification builds consistently select `tests/config_release`. Public checks cover maximum tasks, priority count, tick rate, slicing enablement/quantum, and assertion selection; private checks cover idle stack and alignment. There are no hidden scheduler-configuration defaults. Final product composition must preserve the existing one-config-per-image rule.

## 11. Critical-section and concurrency review

Public pointers are checked before dereference. Descriptor validation occurs before mutation; lifecycle is rechecked after entry. Reservation, object initialization, frame construction, ready insertion, READY transition, commit, magic, and publication are one critical transaction. Every recoverable branch exits through its matching token.

The host port token encodes exact prior mask state and nesting depth. A bounded private LIFO stack detects wrong-order exit in assertion builds; valid nested exits restore the prior state. Task creation neither requests nor performs a context switch while critical. ISR calls are rejected before mutation.

Single-core, startup-only, non-reentrant creation plus the critical recheck prevents duplicate publication or half-created observation. Future `rts_start()` must use the same critical contract when changing INITIALIZED to RUNNING. Host fault injection is single-threaded, one-shot, and host-private.

## 12. Complexity and deterministic RAM

- Application TCB pool RAM is exactly `sizeof(rts_task_pool_t)`, conceptually `RTS_MAX_TASKS * sizeof(struct rts_task) + 2 * sizeof(size_t)` plus any ABI tail/alignment padding.
- Idle ownership adds `sizeof(struct rts_task) + RTS_IDLE_STACK_SIZE_BYTES`, plus kernel-aggregate alignment padding. It is outside application capacity.
- Application stack RAM is entirely caller-owned and equals the sum of supplied stack arrays.
- Host synthetic frame storage is `round_up(sizeof(rts_host_initial_frame_t), RTS_TASK_STACK_ALIGNMENT)` bytes: 48 bytes on the typical 64-bit ABI and 32 bytes on the tested 32-bit ABI.
- Pool initialization is O(`RTS_MAX_TASKS`). Reservation is bounded O(`RTS_MAX_TASKS`). TCB pool-membership verification is another bounded O(`RTS_MAX_TASKS`). Stack construction is O(host frame size), a port constant. Ready insertion, commit, publication, and initialized rollback are O(1).
- Therefore creation critical-section work is bounded O(`RTS_MAX_TASKS`) with two bounded pool scans and constant port/queue work. Capacity exhaustion uses one bounded scan.

The O(`RTS_MAX_TASKS`) search inside critical is accepted for startup-only creation before task execution. It is not a runtime scheduling latency path. Any future runtime-creation proposal must re-evaluate this decision.

## 13. Test and build evidence

Coverage includes pool initialization/reservation/commit/rollback/hint/count; descriptor pointer, entry, priority, stack alignment/size/granularity/overflow; complete TCB initialization; deterministic host frame construction; one and multiple task creation; equal-priority FIFO; mixed priorities; exact capacity and one-beyond; RESET/RUNNING/ISR rejection; recoverable port rollback; repeated failures beyond capacity without leaks; handle/idle/forged membership; assertion contracts; and assertion-enabled/disabled builds.

All focused host tests return zero in both configured assertion modes. Sprint 3 ready/pool/host regressions return zero. Production translation units, including the corrected handle validator and task-creation orchestration, compile as strict C11 freestanding Cortex-M4 objects with warnings promoted to errors. CMake/CTest targets are present; CMake was unavailable in the review environment, so equivalent freestanding WebAssembly executables were run through the bundled Node runtime.

## 14. Remaining non-blocking risks and Sprint 6 dependencies

- The Cortex-M4F port must define the real basic/extended exception frame, initial xPSR, entry/argument registers, LR return trap, callee-saved context, EXC_RETURN, PSP alignment, and lazy/eager FPU policy.
- Target critical masking and ISR-context detection must implement the same token and callable-interrupt-priority semantics.
- Host tests cannot reproduce exception tail chaining, exact interrupt latency, hardware stacking faults, or FPU state.
- Link-map measurement must record actual target TCB pool, idle, stack, and kernel-state RAM.
- Product CMake composition must retain one configuration provider for every object in the final image.

These are target-port/integration obligations, not Sprint 4 transaction defects.

## 15. Exact Sprint 5 assumptions

Sprint 5 may rely on:

1. `rts_kernel_state_get()` returns the one stable static kernel aggregate.
2. Static zero kernel state is RESET and contains canonical-zero pool objects before first initialization.
3. Sprint 5.1 implements `rts_init()` by initializing the pool, ready set, delay queue, port, private idle TCB/stack, and lifecycle without reconstructing Sprint 4 creation logic.
4. Application creation is legal only after init reaches INITIALIZED and before start changes it to RUNNING.
5. Every successful application handle is a stable ALLOCATED, READY, ready-linked typed pool element with valid context metadata.
6. Highest-ready lookup and equal-priority FIFO ordering already include every created application task.
7. The idle TCB is separate, uses priority zero, and must be registered by initialization without consuming application capacity or using the public creation API.
8. The scheduler owns runtime state transitions, current task, selection, switch plans, quantum reload/rotation decisions, and switch requests.
9. The selected port owns critical masking, ISR detection, context mechanics, and first-task transfer; the scheduler never constructs frames.
10. No successful creation requests preemption because no task executes in INITIALIZED.
11. Public yield may operate only in RUNNING task context and must preserve ready-node ownership through ready-queue rotation.
12. Target/Cortex work remains separable from portable scheduler-core host tests.

## 16. Acceptance checklist

| Criterion | Result |
|---|---|
| Exact transaction order and late publication | PASS |
| Complete recoverable rollback and repeated-failure proof | PASS |
| FREE/RESERVED/ALLOCATED and count/hint invariants | PASS |
| Direct stable handles and idle exclusion | PASS |
| Complete committed/free TCB invariants | PASS |
| Ready ownership, FIFO, mixed/highest priority | PASS |
| Host stack contract and isolation | PASS |
| Balanced prior-state/LIFO critical sections | PASS |
| INITIALIZED-only lifecycle and deterministic statuses | PASS |
| Module ownership and no premature scheduler core | PASS |
| Strict C11, alignment, effective type, overflow, padding | PASS |
| Assertion-enabled/disabled behavior | PASS |
| Bounded time and deterministic RAM documented | PASS |
| Sprint 5 assumptions explicit | PASS |

Sprint 4 ACCEPTED — Ready for Sprint 5
