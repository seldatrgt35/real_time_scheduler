# Sprint 6.5 — Public `rts_start()` Integration

## Public contract and status mapping

`rts_start()` is startup/task-context-only and accepts exactly INITIALIZED
lifecycle. RESET returns `RTS_STATUS_INVALID_STATE`, RUNNING returns
`RTS_STATUS_ALREADY_STARTED`, and ISR context returns
`RTS_STATUS_INVALID_CONTEXT`. Internal bootstrap corruption asserts. A
recoverable selected-port rejection before execution transfer is mapped to
`RTS_STATUS_PORT_ERROR` after rollback.

On Cortex-M4F, successful `rts_port_start_first_task()` enters the SVC path and
does not return. The deterministic host port validates and records the same
handoff but executes no task; it returns `RTS_STATUS_OK`, allowing the public
test call to return success with a coherent simulated RUNNING kernel.

## Transaction

The focused implementation is `kernel/scheduler_start.c`. Its sequence is:

1. Reject ISR, RESET, RUNNING, or unknown lifecycle without mutation.
2. Enter the selected port's critical contract and recheck INITIALIZED.
3. Assert bootstrap coherence: null current; valid ready-linked idle; nonempty
   ready set; empty pre-start delay queue; completely inactive switch plan.
4. Call the read-only highest-ready scheduler selector.
5. Require a valid READY runnable selected task.
6. Call the scheduler-owned current-establishment helper. This changes only the
   selected state READY→RUNNING and current pointer; ready links stay unchanged.
7. Commit lifecycle RUNNING.
8. Call `rts_port_start_first_task()` while the portable mutation remains
   protected.

There is no FIFO rotation, switch plan, snapshot, completion, outgoing task, or
PendSV request. Selection naturally chooses the application FIFO head at the
highest numeric priority, or the private priority-zero idle task if no
application task exists.

The RUNNING commit occurs after successful scheduler adoption and immediately
before port handoff activation. This is the first point at which the Cortex SVC
bridge may validate the startup handoff. Portable code contains no PSP, CONTROL,
PRIMASK, SVC, SCB, or NVIC operation. The Cortex port owns the necessary
`cpsie; svc` activation window and immediate SVC-entry masking.

## Rollback

Only a port return other than `RTS_STATUS_OK` is a recoverable post-adoption
failure. Because Cortex success cannot return, such a return is necessarily
before execution transfer. Rollback runs while still protected:

1. Restore lifecycle INITIALIZED.
2. Call the scheduler current-release helper.
3. Verify current is null, selected is READY and remains ready-linked.
4. Restore the prior critical token.
5. Return `RTS_STATUS_PORT_ERROR`.

Queues, FIFO order, delay queue, task pool, handles, stacks, priorities, idle,
and switch-plan state are never rebuilt or reset. A later start retry therefore
selects the same highest FIFO head. The host fail-next-start hook rejects before
recording a handoff and proves this retry behavior.

| Failure point | Lifecycle | Current | Selected state | Port record |
|---|---|---|---|---|
| Public precondition | unchanged | unchanged | unchanged | none |
| Corrupt preflight | INITIALIZED | unchanged | unchanged | none |
| Port pre-transfer rejection | INITIALIZED | null | READY | none |
| Host simulated success | RUNNING | selected | RUNNING | consumed once |
| Cortex SVC success | RUNNING | selected | RUNNING | execution transfers |

## Host and integration verification

Focused host tests cover RESET, ISR, INITIALIZED success, repeated RUNNING
start, idle-only startup, mixed priorities, equal-priority FIFO preservation,
saved-SP identity, balanced critical depth, fault-injected rollback/retry,
unchanged queues/delay/pool/plan, and assertion-enabled corruption guards.

The integration test starts two same-priority application tasks through the
host recorder and then calls public yield. It verifies the start-selected task
is still current/RUNNING, FIFO rotates to its peer, an ordinary A→B plan is
created, and exactly one architecture switch request is recorded. No host task
or context is executed.

## Cortex link and disassembly verification

The ARM link-contract image combines portable kernel units, Cortex stack/start/
switch C units, SVC/PendSV assembly, and test-only definitions for the target
port-initialize, critical, and ISR-detection functions that remain target-
integration work. These stubs are under `tests/support` and cannot enter the
production Cortex library.

Acceptance checks:

- `rts_start` resolves `rts_port_start_first_task` to the Cortex object;
- one strong `SVC_Handler` and one strong `PendSV_Handler` exist;
- all switch/start bridge references resolve and no host port symbol is linked;
- portable `scheduler_start.c` contains no special-register instruction;
- Cortex start still contains `cpsie; svc #0`;
- SVC and PendSV restore sequences remain unchanged;
- restricted objects contain no VFP or heap/runtime dependency.

## Remaining target dependencies

S32K148 integration must provide the real PRIMASK token implementation, IPSR
context detection, port initialization, SHPR priority programming/readback,
STKALIGN validation, vector ownership, and startup/runtime interrupt-source
ordering. Hardware must confirm first-task non-return, Thread PSP/Handler MSP,
enabled task-entry interrupts, and subsequent PendSV switches. SysTick, delay,
time slicing, and timer integration remain outside Sprint 6.5.
