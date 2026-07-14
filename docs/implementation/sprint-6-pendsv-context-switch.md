# Sprint 6.4 — PendSV Ordinary Context Switch

## Scope and ownership

Sprint 6.4 implements Cortex-M4F PendSV request, immutable snapshot handoff,
R4–R11/PSP exchange, and portable switch completion. It does not implement
public start, SysTick, delay/wakeup, time slicing, synchronization, task deletion,
or FP context.

The scheduler remains the sole owner of selection, task-state transitions,
`current_task`, switch generation, and completion. Assembly reads or writes only
PSP, the verified TCB saved-SP field, and the two verified handoff TCB pointers.
It never inspects priority, queues, state, or scheduler globals.

## PendSV request and priority

`rts_port_request_context_switch()` writes the architecture-defined PENDSVSET
bit (`0x10000000`) to SCB ICSR (`0xE000ED04`), then executes DSB and ISB. Cortex
hardware makes this idempotent: repeated writes before service remain one pending
exception. The operation remains safe under PRIMASK; PendSV activates after the
mask clears. No duplicate software-pending flag exists.

The target supplies implemented priority bits and the logical PendSV value.
`port_config.h` rejects out-of-range values and requires PendSV to equal the
lowest implemented logical priority. Final S32K148 integration must program and
read back SHPR without configuring SysTick or unrelated interrupts.

## Handler sequence

PendSV can activate only while PRIMASK is clear. The handler therefore records
no per-task mask and restores the only legal entry state—clear—before return.
Its exact sequence is:

1. `cpsid i` closes scheduler concurrency.
2. Compare exception LR with the named `0xFFFFFFFD` Thread/PSP/basic-frame value;
   any extended or wrong-origin frame is fatal.
3. Read outgoing PSP, which points at the hardware R0 frame.
4. `stmdb` R4–R11 below that frame, producing the outgoing saved SP.
5. Push saved SP and exception LR on MSP, preserving 8-byte C-call alignment.
6. Call the ordinary-C acquire bridge with outgoing saved SP.
7. For a valid handoff, store saved SP through `handoff.from +
   RTS_CM4F_TCB_SAVED_SP_OFFSET` and load incoming SP through `handoff.to` and
   the same symbolic offset.
8. Restore incoming R4–R11 with incrementing LDM and write the advanced pointer,
   now at hardware R0, to PSP.
9. Call the ordinary-C completion bridge with the immutable handoff.
10. Release the MSP local, execute DSB/ISB, clear PRIMASK, synthesize the named
    EXC_RETURN, and `bx lr`.

Hardware alone restores incoming R0–R3, R12, LR, PC, and xPSR. Neither task stack
contains EXC_RETURN, PRIMASK, CONTROL, scheduler metadata, or FP state.

## Register and ABI plan

Before the first C call, R0 holds outgoing PSP/saved SP. Outgoing R4–R11 are
saved before any helper executes. The MSP local holds outgoing SP and exception
LR. After acquire, R0 is the handoff, R1 is outgoing saved SP, and R2 is used for
outgoing/incoming TCB and incoming SP. The handoff replaces the saved-SP MSP
slot before incoming restore.

After R4–R11 are restored, only caller-saved R0–R3 are used. The completion
bridge is ordinary AAPCS C running on aligned MSP; it must preserve R4–R11, so
the incoming task values survive the call. LR is deliberately synthesized after
the final call. PSP is never a C stack in Handler mode.

This is the approved Model B completion ordering: incoming software context and
PSP are restore-ready first, portable state/current completion happens second,
and exception return happens last. A completion failure is fatal and cannot
expose partial state to Thread mode.

## Snapshot bridge and saved-SP validation

The static port handoff contains outgoing/incoming TCB pointers, the complete
immutable Sprint 5 snapshot, and the validated outgoing/incoming saved-SP values.
Acquire calls `rts_scheduler_switch_acquire()`, making PENDING become ACTIVE, and
checks identity, states, distinct tasks, 16-byte alignment, stack bounds, and
room for the 64-byte basic context. It neither publishes outgoing SP nor
completes the switch.

Assembly performs the sole outgoing TCB saved-SP store and incoming saved-SP
load. Completion verifies the same handoff address, active generation identity,
current/outgoing identity, and both saved-SP values before calling the portable
completion operation. It then verifies outgoing READY, incoming RUNNING, new
current, and inactive plan. Queue links, FIFO order, priorities, lifecycle, and
wait state remain untouched.

Debug and release both perform the constant-time bounds checks. More extensive
guard-region/high-water diagnostics remain deferred and are not part of switch
correctness.

## Spurious requests and deferred reselection

A PendSV with no PENDING or ACTIVE plan is a harmless no-op. This covers a stale
hardware pending bit after planner cancellation and duplicate hardware requests.
Although R4–R11 are copied below PSP before acquisition, PSP and the TCB are not
changed; the handler restores its original exception LR and returns to the same
task. Null acquisition while a plan is ACTIVE or malformed is fatal, not
spurious.

Each entry completes at most one snapshot. Preparation during ACTIVE only sets
the portable deferred-reselection flag; completion preserves it. PendSV never
chains or selects. A later portable scheduler evaluation prepares a fresh plan,
whose fresh notification writes PENDSVSET again, so no request is hidden by a
second software flag.

## SVC and FPU compatibility

SVC and PendSV consume the same R4–R11 plus hardware-basic frame, advance PSP by
32 software bytes, use privileged Thread mode on PSP, and synthesize
`0xFFFFFFFD`. Sprint 6.4 does not alter SVC instructions or startup handoff.
PendSV rejects any non-basic EXC_RETURN and contains no S-register/FPSCR/VFP
instruction or lazy-stacking behavior.

## Verification

The host-executable bridge test creates real kernel tasks and queues, prepares a
real switch plan, consumes the actual Cortex C bridge, emulates only the two
symbolic saved-SP memory operations, and invokes real portable completion. It
checks ACTIVE transition, immutable identity, SP publication/selection, stale
handoff rejection, completion states/current, duplicate rejection, deferred
reselection, and unchanged links/priorities/lifecycle. Existing host request
recording models hardware request consumption without executing a task.

ARM objects are compiled for `arm-none-eabi`, Cortex-M4, Thumb, soft-float, and
no-FPU with strict warnings. Disassembly acceptance requires:

```text
mrs psp
stmdb ..., {r4-r11}
BL acquire
symbolic outgoing SP store / incoming SP load
ldmia ..., {r4-r11}
msr psp
BL completion
DSB / ISB / CPSIE
EXC_RETURN 0xFFFFFFFD / BX LR
```

It also verifies PENDSVSET request instructions, exactly one strong SVC and
PendSV vector symbol, acquire before publication, completion after incoming
restore, no software hardware-frame pop, no handler prologue, no heap/runtime
dependency, and no FP instruction.

Future hardware tests use small assembly task routines with distinct R4–R11
patterns across repeated yields; debugger probes verify Thread PSP, Handler MSP,
outgoing/incoming PSP movement, nonoverlapping stacks, and alignment. Optional
latency instrumentation points are PendSV entry, after outgoing save, before
incoming restore, and before exception return; production adds no hooks.

## Remaining dependencies

Public `rts_start()` and S32K148 vector/SHPR integration remain unimplemented.
Target acceptance must prove strong handler ownership against vendor weak
symbols, priority readback, first SVC launch, repeated hardware switches, stack
bounds, and no VFP instructions in the final linked image. SysTick and all tick-
driven scheduling remain outside Sprint 6.4.
