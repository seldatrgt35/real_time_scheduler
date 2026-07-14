# Sprint 3.3 — Delay Queue

## Scope

Sprint 3.3 implements only the portable Version 1 delay-queue data structure. It owns `delay_node` membership and ordering by the absolute wake tick stored in `task->wait.wake_tick`. It does not change task state or wait reason, insert into the ready set, advance time, orchestrate wakeups, request a switch, or call a port.

## Time and ordering model

Ticks are unsigned 32-bit values. Deadline `wake` is due at `now` when the modulo difference `now - wake` is at most `RTS_DELAY_MAX` (`INT32_MAX`). This is equivalent to the approved signed-difference model without relying on implementation-defined unsigned-to-signed conversion.

For ordering, `a` is before `b` when unsigned `a - b` is greater than `RTS_DELAY_MAX`. A difference of exactly `2^31` is ambiguous and is rejected as an internal contract violation. Scheduler-created deadlines are constrained to the accepted half-range, so valid queue contents remain mutually orderable within the active scheduling horizon.

Equal wake ticks are inserted after existing equals, preserving deterministic FIFO order. This is an ordered absolute-tick list, not a delta queue.

## Implemented operations

| Operation | Contract | Complexity |
|---|---|---:|
| `rts_delay_initialize` | Initialize the ordered intrusive list | O(1) |
| `rts_delay_insert` | Insert by modular wake order, after equal deadlines | O(n), bounded by delayed tasks |
| `rts_delay_remove` | Remove a known member and canonicalize its delay node | O(1) |
| `rts_delay_peek_expired` | Return the head task only when due at supplied `now` | O(1) |
| `rts_delay_contains` | Compare node owner with this queue's list | O(1) |
| `rts_tick_deadline_reached` | Evaluate the half-range due predicate | O(1) |

The approved extraction protocol is repeated `peek_expired(now)` followed by `remove(task)`. This drains only due heads and requires no full-list scan in the tick path. No additional pop or earliest accessor was added.

Insertion stores the exact `rts_tcb_t *` in the delay node's opaque `object` backlink; lookup validates and reads that pointer directly, and removal clears it. No container arithmetic or aliasing extension is used.

## Ownership and assertions

Only `delay_node` is mutated. The implementation reads `wait.wake_tick` and does not inspect or change wait reason, task priority/state, ready membership, slice accounting, stack/context, or slot metadata.

Assertions cover null arguments, noncanonical/double insertion, wrong-queue or unlinked removal, malformed list ownership encountered during insertion/peek, ambiguous half-range ordering, and post-mutation ownership. Contract guards return without mutation if an assertion hook returns or assertions are disabled.

## Tests

Focused tests cover empty initialization, unordered insertion producing sorted order, middle removal, canonical unlinking, equal-deadline FIFO, repeated simultaneous-expiry extraction, before/equal/after due checks, wraparound ordering and expiry, maximum accepted horizon, containment, preservation of unrelated TCB fields, double insertion, wrong-queue and unlinked removal, and exact-half-range rejection.

A test-only invariant walker validates owner consistency, reciprocal forward links, endpoints, count, and finite traversal. It is not present in production mutation paths.

## Verification

Normal environments use the committed strict C11 CMake/CTest target. The Codex environment uses freestanding WebAssembly execution because its Windows Clang lacks a host C runtime, and separately compiles production translation units for Cortex-M4 with `-Wall -Wextra -Werror`.

The assertion-enabled and assertion-disabled delay-queue suites each completed with zero failures. `delay_queue.c` also compiled successfully as a freestanding Cortex-M4 translation unit in both configurations with all requested warnings promoted to errors. CMake remains unavailable in this execution environment; its test target is committed for standard developer machines.

## Non-blocking limitations

- Ordered insertion is O(n), but runs outside the tick expiry scan and is statically bounded by the configured task population.
- The queue assumes scheduler-provided wake ticks obey the half-range horizon; exact-half ambiguity is asserted and rejected defensively.
- `peek_expired` intentionally does not unlink or mutate task state. The future scheduler loop owns extraction orchestration.
