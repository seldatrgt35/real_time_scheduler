# Sprint 4.2 — Task Descriptor Validation

**Status:** Implemented; ready for review  
**Scope:** Read-only validation for the future task-creation transaction

## Contract

`rts_task_config_validate()` accepts a public `rts_task_config_t` and a snapshot of the private kernel lifecycle. It returns a public status and performs no mutation. The function does not reserve a pool slot, touch a TCB or stack byte, initialize a node, access a queue, or invoke context-switch behavior.

The lifecycle type is isolated in `lifecycle_internal.h`, allowing both the scheduler state and validator to share the exact constants without making validation depend on queue or scheduler-state layouts. Only `RTS_KERNEL_INITIALIZED` permits task registration.

## Validation order and statuses

1. Null descriptor: `RTS_STATUS_INVALID_ARGUMENT`.
2. Lifecycle other than INITIALIZED: `RTS_STATUS_INVALID_STATE`.
3. Null entry: `RTS_STATUS_INVALID_TASK_CONFIG`.
4. Priority outside `1..RTS_PRIORITY_COUNT-1`: `RTS_STATUS_INVALID_PRIORITY`.
5. Invalid stack pointer, size, bounds, or port constraint: `RTS_STATUS_INVALID_STACK`.
6. Otherwise: `RTS_STATUS_OK`.

The argument pointer is intentionally unrestricted and is never dereferenced. The task entry is compared with null but never called.

## Stack validation

The start address is converted to `uintptr_t` and must be aligned to the public 16-byte `RTS_TASK_STACK_ALIGNMENT`. Size must be nonzero, at least the selected port's minimum, and a multiple of the selected port's size granularity. Addition is accepted only when the byte count is representable in `uintptr_t` and `start + size` cannot overflow. The resulting exclusive top address must also preserve the public alignment.

The portable validator does not hard-code a Cortex-M exception-frame size. Sprint 4.2 adds two read-only port contracts:

- `rts_port_task_stack_minimum_size_bytes()`
- `rts_port_task_stack_size_granularity_bytes()`

The Cortex-M4F port will derive these values from its selected context-frame/FPU policy. The Sprint 4.4 host constructor independently enforces its corresponding constraints before writing the stack; the future Cortex-M4F constructor must do the same. Zero returned by either query is an internal contract violation; assertion builds report it, while defensive execution rejects the stack.

No unrelated pointers are ordered or subtracted, and no heap assumption is made.

## Tests

Focused host tests cover a valid descriptor, nullable argument, null descriptor, RESET/RUNNING lifecycle, null entry, idle/upper-bound priorities, valid highest priority, null/misaligned stack, zero/undersized/non-granular size, end-address overflow, and invalid port-query results. Both assertion-enabled and assertion-disabled configurations are supported.

## Remaining limitations

- ISR-context rejection and output-handle validation belong to the future public `rts_task_create()` orchestration, not descriptor validation.
- Static lifetime, overlap with other stacks, and actual backing-object extent cannot be proven from a C pointer and byte count; they remain application contracts.
- The host port supplies a deterministic test-frame minimum and 16-byte granularity. The Cortex-M4F port must still supply its separately reviewed minimum and granularity before target integration.
- Pool capacity is deliberately not inspected or mutated by this module.
