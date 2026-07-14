# Sprint 4.4 — Architecture Stack Contract and Host Port

**Status:** Implemented; ready for review  
**Scope:** Initial-stack construction contract and deterministic host substitute

## Port contract

`rts_port_stack_initialize()` accepts a validated caller-owned byte region, entry function, and argument. It returns `rts_port_stack_result_t` containing an explicit status and `saved_stack_pointer`. A successful result has `RTS_STATUS_OK` and a nonnull, aligned saved SP inside the supplied region. Every failure returns a null saved SP and writes no stack byte.

The two read-only queries introduced in Sprint 4.2 are implemented by the host port:

- `rts_port_task_stack_minimum_size_bytes()` returns the aligned storage required by the host initial frame.
- `rts_port_task_stack_size_granularity_bytes()` returns the 16-byte host stack granularity.

The constructor independently checks nonnull stack and entry, public start alignment, nonzero/minimum/granular size, integer representability, end-address overflow, and usable-top alignment. A null entry returns `RTS_STATUS_INVALID_TASK_CONFIG`; stack-region failures return `RTS_STATUS_INVALID_STACK`.

## Host initial frame

The host substitute models a downward-growing stack. It reserves an aligned block at the exclusive top of the supplied region and returns the block start as the saved SP. Its private frame records:

- fixed magic and layout version;
- the exact entry-function pointer;
- the exact argument pointer, including null;
- the host task-return trap;
- a zero reserved word.

The frame is not an emulation of Cortex-M register stacking and is never executed. It exists only to verify architecture-boundary data flow deterministically. The host return trap invokes the fatal assertion hook and cannot return if accidentally reached.

`ports/host/port_internal.h` exposes the host-only frame description and a byte-decoding helper solely to white-box port tests. Neither is included by the portable kernel or public API.

## Strict C11 object access

Application stacks are declared byte arrays. Treating bytes in such an array as a directly overlaid frame structure would conflict with the declared array element type. The host port therefore creates a fully initialized local typed frame, zeroes the reserved frame storage, and copies each field's object representation through `unsigned char` accesses at its standard `offsetof` position. Tests decode by the inverse byte copy into a real typed local object. No unrelated pointer ordering, aliasing extension, packed structure, or disabled strict-aliasing mode is used.

Padding and unused aligned frame bytes remain deterministically zero because the full reserved block is cleared and only named field representations overwrite it; unspecified local-structure padding is never copied.

## Ownership and isolation

The host port writes only its frame storage. It does not access a TCB, pool, queue, scheduler lifecycle, current task, or context-switch plan. It does not publish a handle, request switching, execute the entry, or emulate host task execution. The portable creation transaction will later store the returned saved SP.

## Tests

Focused tests cover query consistency, minimum/alignment constraints, exact saved-SP placement, decoded entry/argument/return-trap fields, zero reserved data, unchanged bytes below the frame, null argument, deterministic frame bytes across different stack buffers, null/misaligned/undersized/non-granular/overflowing regions, null entry, null result-reader arguments, no entry execution, and no writes on failure.

The host port and tests build with strict C11 warnings-as-errors under both assertion configurations. No CMSIS, NXP SDK, OS threading, dynamic allocation, or host context library is used.

## Remaining limitations

- The host frame intentionally does not reproduce Cortex-M4F hardware exception or FPU context layout.
- Cortex-M4F minimum size, granularity, initial xPSR/register values, exception return, and FPU policy require a separate target-port implementation and review.
- Storing the successful saved SP into a RESERVED TCB remains part of the future task-creation transaction.
- Sprint 4.5 adds the host ISR-query and critical-section subset required by task creation. Switch requests and first-task startup remain unimplemented.
