# Sprint 6.1 — Cortex-M4F Execution Contract

**Status:** Approved architecture baseline  
**Scope:** ARMv7E-M execution ABI and implementation contracts; no handlers or
hardware execution are implemented in Sprint 6.1.

## 1. Processor and stack ownership

Scheduler-managed tasks execute in privileged Thread mode using PSP. Exceptions
execute using MSP. Reset/startup code begins in Thread mode on MSP, initializes
MSP, `.data`, `.bss`, the vector table location, and the C runtime before the
kernel is called. The first task is entered through SVC; every ordinary task
transfer is performed by PendSV. PendSV has the lowest implemented configurable
exception priority. SysTick is outside this sprint.

The Cortex port alone reads or writes PSP, MSP, CONTROL, PRIMASK, SCB exception
pending/priority fields, and target interrupt-priority facilities. The portable
kernel sees only the contracts in `kernel/port.h`; it never includes Cortex
register definitions.

## 2. Version 1 context and FPU policy

Exception entry supplies the basic hardware frame: R0, R1, R2, R3, R12, LR,
PC, and xPSR. The port software frame supplies R4–R11. EXC_RETURN is not stored
per task. With extended frames forbidden, restore code loads the named constant
`RTS_CM4F_EXC_RETURN_THREAD_PSP_BASIC` into LR immediately before exception
return.

Floating-point execution is unsupported in scheduler-managed tasks, portable
kernel code, the Cortex port, and scheduler-aware interrupt handlers. No code in
those domains may execute a VFP instruction or depend on FP context retention.
Lazy FP stacking is disabled during port initialization and an extended-frame
EXC_RETURN is a fatal contract violation.

The complete image uses the AAPCS soft-float ABI. Kernel, port, application task,
and linked library objects must be ABI-compatible; an S32 SDK built for
`-mfloat-abi=hard` cannot be linked and must be rebuilt or replaced. GCC/Clang
kernel and port flags are:

```text
Common: -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
GCC restricted C:   -mgeneral-regs-only
Clang restricted C: -mfpu=none
Common C: -ffreestanding -fno-builtin -std=c11
Warnings: -Wall -Wextra -Werror -Wpedantic
```

Application and ISR objects also use `-mfloat-abi=soft` and must contain no FP
operations. GCC uses `-mgeneral-regs-only`; Clang ARM uses `-mfpu=none`, because
Clang does not accept the GCC flag for this target. The corresponding restriction
is mandatory for kernel/port C units and recommended for restricted application/
ISR units. Build logic rejects `__ARM_PCS_VFP` and nonzero `__ARM_FP`.
Acceptance disassembles every kernel/port object and rejects VFP mnemonics. No
correctness rule depends on `-fno-strict-aliasing`.

Future versions may adopt one separately approved image-wide policy: lazy plus
conditional S16–S31 preservation, explicit conditional preservation using
EXC_RETURN, or unconditional FP preservation. Such a change must extend the
frame and EXC_RETURN model atomically; it is not backward-compatible with this
Version 1 context.

## 3. Initial stack frame

Stacks are full-descending. The validated region base and byte count are each
16-byte aligned. The port starts at `stack_buffer + stack_size_bytes`, subtracts
64 bytes, writes the frame below, and returns that low address as `saved_sp`.
The exact indexed layout from final saved-SP upward is:

| Word | Address | Owner | Register | Initial value |
|---:|---:|---|---|---|
| 0 | `saved_sp + 0x00` | Software | R4 | `0` |
| 1 | `saved_sp + 0x04` | Software | R5 | `0` |
| 2 | `saved_sp + 0x08` | Software | R6 | `0` |
| 3 | `saved_sp + 0x0C` | Software | R7 | `0` |
| 4 | `saved_sp + 0x10` | Software | R8 | `0` |
| 5 | `saved_sp + 0x14` | Software | R9 | `0` |
| 6 | `saved_sp + 0x18` | Software | R10 | `0` |
| 7 | `saved_sp + 0x1C` | Software | R11 | `0` |
| 8 | `saved_sp + 0x20` | Hardware | R0 | task argument |
| 9 | `saved_sp + 0x24` | Hardware | R1 | `0` |
| 10 | `saved_sp + 0x28` | Hardware | R2 | `0` |
| 11 | `saved_sp + 0x2C` | Hardware | R3 | `0` |
| 12 | `saved_sp + 0x30` | Hardware | R12 | `0` |
| 13 | `saved_sp + 0x34` | Hardware | LR | private return trap |
| 14 | `saved_sp + 0x38` | Hardware | PC | task entry |
| 15 | `saved_sp + 0x3C` | Hardware | xPSR | `0x01000000` |

Total size is 16 words/64 bytes. The initial top, saved SP, software-frame end,
and hardware-frame end are 16-byte aligned; this exceeds the ARM architectural
8-byte requirement. Port initialization requires CCR.STKALIGN behavior that
preserves 8-byte exception frames and rejects conflicting startup settings.
After software restore PSP points at `saved_sp + 0x20`; hardware exception return
consumes 32 bytes and leaves the task PSP at its original 16-byte-aligned top.
The 16-byte guarantee applies to the initial frame. Once Thread mode is
executing, the AAPCS permits a task prologue to move PSP to an address that is
only 8-byte aligned. PendSV therefore validates a runtime saved-SP at the
Cortex-M architectural 8-byte boundary and must not reject a legal 8-byte,
non-16-byte PSP; the caller-owned region and initial frame remain 16-byte
aligned.
The port's structural minimum is therefore 64 bytes with 16-byte granularity.
This only guarantees frame construction; each application must budget additional
stack for its call depth, local data, exceptions, and safety margin.

Both SVC first restore and PendSV incoming restore consume this identical frame.
Startup differs only because it has no outgoing context.

## 4. EXC_RETURN and function-pointer encoding

`RTS_CM4F_EXC_RETURN_THREAD_PSP_BASIC` is `0xFFFFFFFD`: return to Thread mode,
use PSP, and consume a basic (non-FP) frame. SVC and PendSV both synthesize this
constant in LR after their final C call. Raw copies of the literal are forbidden.

The Cortex port supports GCC and Clang ARM EABI with 32-bit code pointers and
Thumb function-pointer encoding. Port-local stack construction copies the
32-bit object representation of `rts_task_entry_t` and the private trap function
pointer into temporary `uint32_t` words using byte-copy semantics, then writes
them to PC and LR slots. Portable code performs no function/object pointer
reinterpretation. Compile-time size checks and a link/first-start test verify the
toolchain assumption. xPSR.T is always set; Thumb symbols must resolve to valid
Thumb code. Unsupported pointer representations fail the port build.

The trap is private, `_Noreturn`, enters the fatal/assert path, and loops forever
if that path returns. It performs no deletion and exposes no public symbol.

## 5. Saved-SP access contract

Version 1 uses **verified assembler offsets (Model B)**. Preprocessed assembly
and C include `port_offsets.h`. C statically verifies
`offsetof(struct rts_task, saved_stack_pointer)` against
`RTS_CM4F_TCB_SAVED_SP_OFFSET`; assembly uses only that symbolic value.

Model A is fast but an unlabelled `[tcb,#0]` hides coupling. Model C is testable
but introduces C-call ABI and latency into two simple loads/stores. Model B has
Model A performance, makes compiler/layout coupling explicit, and fails the C
build if the layout moves. Generated offsets may replace the shared checked
header later without changing assembly call sites.

## 6. PendSV snapshot bridge and ordering

PendSV never selects, reads queues/priorities, changes states/current, or creates
a plan. A port-owned, non-reentrant `rts_cm4f_switch_handoff_t` contains the
immutable Sprint 5 snapshot plus direct outgoing/incoming TCB pointers. Its
offsets are shared and statically verified.

The future sequence is:

1. PendSV enters on MSP and preserves its exception LR on MSP for ABI-safe C
   calls; PRIMASK is asserted.
2. Ordinary-C `rts_cm4f_switch_bridge_acquire()` calls the portable snapshot
   acquisition routine and returns the stable handoff pointer, or null for a
   cancelled/no-plan request.
3. The `.S` handler reads PSP, pushes R4–R11 to the outgoing task stack, and
   stores the new PSP through the outgoing TCB symbolic offset.
4. It loads incoming saved SP, restores R4–R11, and writes the resulting PSP.
5. Ordinary-C `rts_cm4f_switch_bridge_complete(handoff)` validates and completes
   the exact snapshot while still on MSP and masked.
6. Assembly loads the named EXC_RETURN constant into LR, executes required DSB/
   ISB ordering, restores the prior mask as specified by the exception wrapper,
   and returns to the incoming basic frame.

Completion occurs **after incoming software context and PSP are restore-ready,
but before exception return**. Thus scheduler-visible current changes only when
the incoming context is prepared, while faults before preparation leave the old
ownership intact. Failure in completion is fatal and never exposes a partially
switched task to Thread mode. The C completion call honors AAPCS and therefore
preserves restored R4–R11.

The bridge uses ordinary C, not naked C. The handler is pure preprocessed `.S`.
Its MSP is 8-byte aligned at every C call. R0–R3/R12 are caller-saved and carry
arguments/results; R4–R11 are callee-saved; exception LR is explicitly protected
across `BL`. Before acquisition the handler does not modify R4–R11; AAPCS forces
the bridge to preserve any of them it uses on MSP, so the later explicit PSP save
still observes the outgoing task values. No C call is made while task values in
caller-saved registers need preservation.

## 7. SVC first-task startup

Future `rts_start()` validates INITIALIZED/task context, enters the kernel
critical region, selects the highest ready task, establishes it as current and
RUNNING, changes lifecycle to RUNNING, and asks the port to start it. Before the
SVC instruction the port validates exception configuration and publishes a
port-owned startup handoff pointing to that current TCB. Recoverable errors roll
back the portable start transaction before SVC.

PRIMASK blocks activation of SVC, so `rts_port_start_first_task()` must not issue
`svc` while masked. Startup preparation occurs masked, requires that the caller's
pre-start PRIMASK was clear, publishes the handoff, restores PRIMASK to clear,
and executes `svc` in the immediately following instruction. SVC masks with
PRIMASK on handler entry before touching scheduler/port state. Until that point
all scheduler-aware interrupt sources (including future SysTick) remain disabled;
any permitted higher-urgency application interrupt is forbidden to call the
scheduler. `rts_port_start_first_task()` is non-returning after the instruction.
SVC reads only the startup handoff; it does
not reselect and does not call switch acquisition/completion. It loads saved SP,
restores R4–R11, sets PSP, sets CONTROL.SPSEL while retaining privileged Thread
mode, executes ISB, restores the startup mask policy, loads the named EXC_RETURN,
and returns to the basic frame. If SVC returns to the instruction following
`svc`, the port enters the fatal path. There is no fake null-outgoing snapshot.

No public recovery exists after successful exception return. Invalid port
configuration or stack/start handoff detected before SVC returns
`RTS_STATUS_PORT_ERROR`; malformed state in SVC is fatal.

## 8. Critical masking and exception priorities

Sprint 6 uses **PRIMASK**. `rts_port_critical_enter()` captures the exact prior
PRIMASK value, disables configurable interrupts, and returns an opaque token.
Exit validates LIFO ownership and restores that exact value. PendSV may become
pending while masked and executes after unmasking. PRIMASK never appears in a
public type or portable kernel source.

This is deliberately simple and maximizes first-hardware correctness, but it
increases worst-case latency by masking every configurable interrupt. The token
API remains capable of later migration to BASEPRI after a kernel-aware interrupt
ceiling and priority audit are approved.

The target supplies implemented priority bits and logical SVC/PendSV priorities.
The port validates the range and requires PendSV to equal the lowest logical
priority. SVC must have greater urgency than every enabled scheduler-aware
interrupt so first launch cannot be displaced by kernel work. The Cortex port
owns SHPR programming and readback. The S32K148 target
owns the device's implemented-bit fact and vector/startup integration. The
application may choose SVC and future SysTick/kernel-aware interrupt priorities
within the validated target policy, but cannot override PendSV's lowest value.
The portable kernel owns none of these numeric values. Future SysTick must not
run below PendSV and its scheduler-aware priority must comply with the then-
approved masking policy.

## 9. Assembly/C symbol and implementation split

`PendSV_Handler` and `SVC_Handler` are vector-visible functions implemented in a
preprocessed `.S` file with no compiler prologue. PSP/CONTROL operations,
software register save/restore, exception return, and barriers remain assembly.
Port initialization, frame construction, priority validation, snapshot/startup
handoffs, fatal trap, and scheduler acquire/complete calls are ordinary C.
Naked C handlers and inline-assembly context switches are forbidden.

Internal symbols use the `rts_cm4f_` prefix. Public portable port symbols retain
the existing `rts_port_` names. Startup must map the exact vector names
`SVC_Handler` and `PendSV_Handler` and must not provide competing SDK handlers.

## 10. Compile-time, linker, and startup checks

The Cortex-only private header checks 8-bit bytes, 32-bit words/object pointers/
function pointers, Thumb compilation, absence of hard-float PCS, frame and stack
alignment, saved-SP/handoff offsets, priority-bit range, and lowest PendSV
priority. The 64-byte frame must fit the port-reported minimum stack size.
Target configuration additionally verifies the device priority-bit declaration
against CMSIS/device headers and verifies CCR.STKALIGN during initialization.
These checks are never included in host targets.

Target integration must provide a valid initialized MSP, initialized `.data` and
`.bss`, correct VTOR/vector location where VTOR is used, 8-byte MSP alignment at
exception/C-call boundaries, SVC/PendSV vector ownership, configurable SHPR
access, scheduler-aware interrupt sources disabled until SVC masks on first
entry, and no SDK/bootloader scheduler-handler collision. Link-time map checks
must show one definition for each handler and trap.

Recommended release flags retain strict warnings and may add section GC and
optimization. LTO is disabled for the first hardware acceptance because handler
and disassembly auditing must be transparent; enabling it later requires the
same post-link checks. No frame-pointer option is relied upon. `.S` files use the
C preprocessor so shared constants are authoritative.

## 11. Host/Cortex verification matrix

| Concern | Host port | Cortex-M4F port |
|---|---|---|
| Initial frame | Host metadata record | Exact 16-word exception-compatible frame |
| Switch request | Recorded counter/flag | SCB PendSV-set pending bit plus barriers |
| Completion | Test explicitly acquires/completes | PendSV bridge completes before return |
| Critical token | Nesting model | Exact PRIMASK snapshot/restore |
| ISR detection | Test-controlled boolean | IPSR nonzero |
| Task stack | Never executed | PSP in Thread mode |
| Handler stack | Not modeled | MSP in SVC/PendSV |

Host tests prove portable scheduler ownership and plan semantics, not hardware
ABI correctness.

Hardware tests must demonstrate SVC first launch; PSP task/MSP handler use;
R4–R11 retention over repeated switches; R0 argument delivery; return-trap
entry; Thumb xPSR; lowest PendSV priority; yield-to-PendSV behavior; two-task
alternation; mixed-priority selection; exact PRIMASK restoration; persistent
8/16-byte alignment; and fatal handling of unexpected task return.

Static/post-link checks reject VFP instructions in restricted objects, verify
handler vector references and unique symbols, verify frame/offset constants and
`0xFFFFFFFD`, inspect exact R4–R11 save/restore lists, require PSP/MSP/CONTROL
instructions and barriers, and confirm handlers have no compiler-generated
prologue. Hardware debugger tests inspect PSP/MSP and stacked words at first
launch and repeated PendSV entry.

## 12. Failure model

Recoverable before transfer: port initialization failure, invalid priority/
STKALIGN configuration, invalid stack construction, and `rts_start()`/startup
handoff failure before SVC. Fatal/internal: malformed or stale active snapshot,
null/invalid current in RUNNING, invalid saved SP/alignment, corrupted frame,
extended FP frame, SVC unexpectedly returning, switch completion failure, and
task return. No public recovery is promised after the first task begins.

## Approved Sprint 6.1 Cortex-M4F Execution Baseline

- **FPU policy:** FP instructions and FP-dependent tasks/ISRs are unsupported;
  no FP context exists.
- **Float ABI/toolchain policy:** the complete image uses AAPCS soft-float;
  restricted GCC units add `-mgeneral-regs-only`, restricted Clang units add
  `-mfpu=none`, and hard-float/FP-enabled objects are rejected.
- **Critical masking mechanism:** PRIMASK with exact prior-state token restore.
- **EXC_RETURN storage:** not stored per task; both handlers synthesize the named
  `0xFFFFFFFD` Thread/PSP/basic-frame value at final return.
- **Initial frame layout:** one 64-byte, 16-word frame: R4–R11 then the basic
  hardware R0–xPSR frame exactly as tabulated above.
- **Saved-SP access model:** Model B, shared symbolic assembler offsets verified
  against C `offsetof` assertions.
- **Snapshot bridge model:** a port-owned immutable handoff populated and
  completed by ordinary-C bridge helpers, consumed by pure assembly PendSV.
- **SVC startup model:** dedicated no-outgoing startup handoff; current/state and
  RUNNING lifecycle are established under PRIMASK, the mask is cleared directly
  before the synchronous SVC, the handler remasks on entry, and no fake switch
  completion is used.
- **PendSV completion ordering:** restore incoming software context and PSP,
  complete the immutable snapshot while masked, then exception-return.
- **Exception priority ownership:** target supplies implemented bits; Cortex port
  validates/programs SVC/PendSV and forces PendSV lowest; portable kernel owns no
  NVIC values.
- **Assembly/C implementation split:** handlers, register transfer, special
  registers, barriers, and exception return are `.S`; validation, frame build,
  handoffs, and scheduler calls are ordinary C; naked C is forbidden.
