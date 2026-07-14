# Sprint 3.2 — Fixed-Priority Ready Queue

## Scope

Sprint 3.2 implements the private Version 1 ready-set mechanism only. It owns `ready_node` membership and fixed-priority/FIFO ordering. It does not change task state or wait metadata, decide whether to preempt, account time slices, update the current task, or call the architecture port.

## Representation

The ready set contains one intrusive FIFO list for each of `RTS_PRIORITY_COUNT` priorities and a `uint32_t` bitmap with `ceil(RTS_PRIORITY_COUNT / 32)` words. Numerically larger priority values win. Priority zero is supported by the mechanism for the private idle task; enforcing that idle is its only member remains a scheduler-core invariant.

The bitmap bit for a priority is set exactly when its FIFO is nonempty. Insertion appends at the priority tail. Removing the last member clears the bit. Equal-priority rotation removes the list head and appends that same node, preserving all other FIFO order.

## Implemented operations

| Operation | Contract | Complexity |
|---|---|---:|
| `rts_ready_initialize` | Initialize every priority FIFO and clear the bitmap | O(`RTS_PRIORITY_COUNT`) once at startup |
| `rts_ready_insert` | Append one unlinked task at its immutable priority | O(1) |
| `rts_ready_remove` | Remove a member and clear an empty-priority bit | O(1) |
| `rts_ready_peek_highest` | Return the head task at the highest set priority | O(`RTS_READY_BITMAP_WORDS + 32`), compile-time bounded |
| `rts_ready_has_peer` | Test whether a contained task's FIFO has another member | O(1) |
| `rts_ready_contains` | Compare the node owner with the expected priority FIFO | O(1) |
| `rts_ready_rotate` | Move a FIFO head to its tail when at least two members exist | O(1) |

Highest lookup uses bounded standard-C scans and no compiler bit-scan intrinsic. Insertion stores the exact `rts_tcb_t *` in the ready node's opaque `object` backlink; lookup validates and reads that pointer directly, and removal clears it. No container arithmetic, aliasing extension, or public conversion API is used.

## Ownership and assertions

Only `ready_node` is mutated. Priority is inspected but not changed; task state, wait reason, wake tick, slice accounting, stack/context fields, and slot metadata are untouched.

The implementation asserts null arguments, invalid priorities, noncanonical insertion nodes, wrong-set removal, rotation without a peer, bitmap/list inconsistency during lookup, and containing-task priority mismatch. Contract guards return without mutation if the assertion hook returns or assertions are compiled out, so valid behavior never depends on assertion side effects.

## Tests

Focused tests cover empty initialization, priority zero, priorities spanning bitmap boundaries 31/32 and 64, highest numeric selection, bitmap set/clear behavior, equal-priority FIFO order, head removal, peer detection, membership, three-task rotation, preservation of non-ready TCB fields, independent ready sets, invalid priority, double insertion, wrong-set/unlinked removal, and rotation without a peer.

A test-only validator checks every bitmap/list correspondence, local list ownership, forward count, endpoints, and finite traversal. It is absent from production code.

## Verification

Normal developer builds use the repository CMake/CTest targets with C11 extensions disabled and warnings promoted to errors. In the Codex environment, tests are also suitable for freestanding WebAssembly execution because the available Windows Clang has no host C runtime. The production translation units are compiled independently for Cortex-M4 as a cross-target syntax/object check.

The assertion-enabled and assertion-disabled ready-set suites each completed with zero failures under freestanding WebAssembly execution. `ready_queue.c` compiled successfully for Cortex-M4 in both configurations using `-Wall -Wextra -Werror`, and the Sprint 3.1 intrusive-list suite was rerun after expanding the test priority count to 65 with zero regressions. CMake itself remains unavailable in this execution environment; the committed CMake/CTest targets are intended for normal developer machines.

## Non-blocking limitations

- Highest selection uses a portable bounded scan rather than a target bit-scan instruction; a measured, semantics-preserving optimization may be reviewed later.
- Initialization cost and ready-set RAM scale with configured priority count, both statically bounded.
- The ready set deliberately cannot enforce task-state transitions or the rule that idle is the only priority-zero task; those belong to the scheduler core.
