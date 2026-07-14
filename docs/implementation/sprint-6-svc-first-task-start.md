# Sprint 6.3 — SVC First-Task Start

## Scope

Sprint 6.3 implements the Cortex-M4F architecture side of first-task launch. It
adds a single-start handoff, an SVC trigger, and `SVC_Handler`. It does not add
public `rts_start()`, task selection, switch snapshots, PendSV, SysTick, timer
logic, or execution-port FPU support.

## Startup handoff and ownership

The static port-private handoff contains only:

| Offset | Field | Purpose |
|---:|---|---|
| `0x00` | `first_task` | Scheduler-owned current TCB |
| `0x04` | `saved_stack_pointer` | Initial frame address at R4 |
| `0x08` | `cookie` | Private corruption/staleness check |
| `0x0C` | `valid` | One active/consumed marker |

C `_Static_assert`s verify every shared offset. Preparation requires RUNNING
lifecycle, a non-null scheduler current task in RUNNING state, a valid scheduler
current invariant, a non-null 16-byte-aligned saved SP, and no pending or active
ordinary switch plan. It copies current and saved SP but never writes the task,
current pointer, lifecycle, queues, pool, priority, or switch plan.

`rts_cm4f_start_attempted` is monotonic for the boot lifetime. Invalid attempts
do not set it; the first valid preparation sets it permanently. The handoff is
immutable between trigger and SVC consumption. Consumption validates cookie,
valid marker, exact current identity, saved-SP identity, and all current-task
preconditions, then clears only `valid` before returning the saved SP.

The existing narrow private port API remains
`rts_status_t rts_port_start_first_task(void)`. It reads no policy input: future
portable `rts_start()` must already have selected and adopted current.

## Portable ordering and failure model

The frozen future portable sequence is:

1. Require INITIALIZED lifecycle and task context.
2. Enter the PRIMASK critical contract from an initially unmasked caller.
3. Select the highest ready task and establish it as current/RUNNING.
4. Commit lifecycle RUNNING.
5. Call `rts_port_start_first_task()` while state remains protected.
6. Never return after the valid handoff triggers SVC.

The lifecycle/current commit precedes SVC, matching Sprint 6.1. Before the
trigger, null/malformed current, invalid saved SP, duplicate startup, or an
ordinary switch plan returns `RTS_STATUS_INVALID_STATE`; portable startup may
roll back before invoking SVC. Target exception-configuration validation remains
part of the future Cortex port-initialization integration. Once `svc` executes,
bad handoff/restore state and any unexpected return are fatal.

## Necessary SVC activation window

PRIMASK blocks SVC activation. The pure-assembly trigger executes exactly:

```text
cpsie i
svc #0
b rts_cm4f_start_fatal   ; only if transfer unexpectedly returns
```

SVC number zero is fixed but not decoded because Version 1 has no syscall
dispatcher and uses SVC only for first launch. The caller must have entered with
PRIMASK clear before the startup critical section. All scheduler-aware interrupt
sources, including future SysTick, remain disabled across the two-instruction
window; any enabled higher-urgency application ISR is forbidden to call the
scheduler. The first SVC instruction is `cpsid i`, closing the window before
handoff validation.

## Handler and ABI sequence

`SVC_Handler` is pure preprocessed Thumb assembly and executes on MSP. It pushes
`{r0,lr}` so MSP stays 8-byte aligned while ordinary-C
`rts_cm4f_start_handoff_consume()` runs. This call occurs before incoming
R4–R11 restoration. The handler then performs:

```text
ldmia saved_sp!, {r4-r11}
msr   psp, saved_sp
mrs   r1, control
bic   r1, r1, #(CONTROL.nPRIV | CONTROL.FPCA)
orr   r1, r1, #CONTROL.SPSEL
msr   control, r1
isb
cpsie i
load  lr, RTS_CM4F_EXC_RETURN_THREAD_PSP_BASIC
bx    lr
```

The incremented SP points at word 8/R0, the 8-byte-aligned hardware basic frame.
Software does not pop R0–xPSR. Exception return restores R0–R3, R12, task-return
LR, entry PC, and Thumb xPSR, leaving task PSP above the complete 64-byte frame.
The TCB saved SP intentionally remains at the pre-restore R4 address until the
first future PendSV save updates it.

CONTROL explicitly clears nPRIV, so tasks remain privileged; sets SPSEL, so
Thread mode uses PSP; and clears FPCA under the no-FPU policy. MSP remains the
exception stack. `0xFFFFFFFD` is supplied through the approved named assembler
constant and selects Thread/PSP/basic-frame exception return.

The handler enables configurable interrupts immediately before exception
return. Thus the first task begins with PRIMASK clear and no mask is stored per
task. Scheduler-aware sources may be enabled only after the startup integration
has established their safe runtime ordering.

## Build and verification

The Cortex CMake target now enables preprocessed ASM and compiles `port_asm.S`,
`port_start.c`, and `port_stack.c` only when the Cortex port option is selected.
Host builds never compile the ARM assembly.

`tests/unit/test_cortex_m4f_start_handoff.c` supplies a Cortex-linkable private
contract fixture. It checks RESET/non-RUNNING/null/misaligned/current-invalid
cases, pending/active-plan rejection, exact handoff contents, no scheduler state
mutation, consume semantics, and duplicate-start rejection. It does not execute
SVC on the host.

The host port has a separate non-executing startup recorder. It applies the same
current/lifecycle/saved-SP/plan preconditions, records and consumes exactly one
request, and returns `RTS_STATUS_OK` to its focused test without creating a host
thread or pretending that an exception transfer occurred. The host test verifies
single-request identity, duplicate rejection, unchanged current/state, and no
switch-plan mutation; it makes no Cortex ABI claim.

ARM assembly acceptance uses:

```text
llvm-objdump -d port_asm.o
llvm-nm port_asm.o port_start.o
```

Checks require one strong `SVC_Handler`, no `PendSV_Handler`, `cpsie; svc #0` in
the trigger, `cpsid` first in the handler, an 8-byte MSP bridge save, exact
R4–R11 LDM restore, PSP and CONTROL writes, ISB, final `cpsie`, basic-frame
EXC_RETURN and `bx lr`. They reject software hardware-frame pops, FP/VFP
instructions, heap/runtime helpers, and compiler prologues in the handler.

The target vector table must bind its SVC slot to this strong symbol and the link
map must show exactly one definition; a vendor weak/default handler must not win.
An S32K148 hardware test must confirm MSP in SVC, PSP in task, R0 argument
delivery, privileged CONTROL, clear PRIMASK on entry, and intentional task return
reaching the private fatal trap.

## Remaining dependencies

Public `rts_start()` must implement the portable transaction and rollback before
the SVC point. Cortex port initialization must validate priority/vector/STKALIGN
configuration and keep scheduler-aware interrupt sources disabled through the
activation window. PendSV remains entirely absent and will later own ordinary
context switching and the first saved-SP update.
