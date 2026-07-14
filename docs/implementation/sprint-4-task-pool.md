# Sprint 4.1 — Task Pool Manager

**Status:** Implemented; ready for architecture and code review  
**Scope:** Private application-task TCB ownership only

## Ownership and boundary

`rts_task_pool_t` owns exactly `RTS_MAX_TASKS` normal, typed `struct rts_task` objects, the number of committed application slots, and a bounded-search hint. It is embedded in the kernel state. The idle TCB remains a separate object and never consumes a pool slot.

The pool manager mutates only `slot_state`, `allocated_count`, and `next_free_hint`. Initialization deliberately does not clear or initialize any other TCB field. Task descriptors, stack frames, task state, wait metadata, list nodes, queues, scheduler lifecycle, and architecture state remain outside this module.

Slot identity is a checked array index. Lookup returns the address of the actual typed pool element; it performs no pointer reinterpretation or container arithmetic. This makes lookup, commit, and rollback strict-C11 O(1) operations.

## Slot lifecycle

```text
FREE --reserve--> RESERVED --commit--> ALLOCATED
                         |
                         +--rollback--> FREE
```

`RESERVED` is unpublished transaction ownership. It is excluded from `allocated_count`. Commit is the only operation that increments the count. Rollback does not change the count and moves the hint to the returned slot. Because Version 1 has no task deletion, `ALLOCATED` has no outgoing transition.

## Allocation algorithm

Reservation begins at `next_free_hint`, examines at most `RTS_MAX_TASKS` slots, and wraps at the array boundary. The first `FREE` slot becomes `RESERVED`; the hint advances to its successor. If no free slot exists, reservation returns `false` and the output index is `RTS_TASK_POOL_INVALID_INDEX`.

The hint is an optimization, not ownership truth. Slot states remain authoritative. An assertion-enabled build detects a hint outside the pool before traversal.

## Operations and complexity

| Operation | Result | Worst-case time |
|---|---|---|
| Initialize | Marks every slot `FREE`; resets count and hint | O(`RTS_MAX_TASKS`) |
| Reserve | Bounded first-free scan; `FREE -> RESERVED` | O(`RTS_MAX_TASKS`) |
| Commit | `RESERVED -> ALLOCATED`; increments count | O(1) |
| Rollback | `RESERVED -> FREE`; retargets hint | O(1) |
| Lookup | Checked index to actual TCB address | O(1) |
| Count/hint query | Returns validated metadata | O(1) |

No operation allocates memory, recurses, touches a queue, or calls the architecture port.

## Assertions and defensive behavior

Assertion-enabled builds detect null pool/output arguments, out-of-range slot indices, commit or rollback from the wrong state, allocation-count overflow, and hint corruption. Guards return without mutation if an assertion handler returns during a host test. Capacity exhaustion is an ordinary result, not an assertion.

## Focused tests

`tests/unit/test_task_pool.c` covers initialization, preservation of unrelated TCB fields, first-slot reservation, complete allocation, exhaustion, commit, rollback, committed-count semantics, hint advance/wrap/reuse, direct typed lookup, and reserved-slot exclusion from the committed count. Assertion builds additionally exercise double commit, double rollback, commit of a free slot, invalid index, null arguments, corrupted hint, and count overflow.

The test is registered as `rts_task_pool` in CMake and is built with the same strict warning policy as the Sprint 3 modules.

## Remaining limitations

- Concurrency protection belongs to the later task-creation transaction; the pool manager does not enter critical sections itself.
- The module does not validate or initialize task contents.
- There is no release operation for committed slots because Version 1 has no task deletion.
- Pool-handle validity and public task-creation orchestration remain future Sprint 4 work.
