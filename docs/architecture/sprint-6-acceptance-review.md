# Sprint 6 Final Integration Review and Acceptance Gate

**Review date:** 2026-07-16
**Scope:** Sprint 6.1 through Sprint 6.6
**Disposition:** Architecturally accepted for Sprint 7; SDK-backed target build
and physical S32K148 execution remain external acceptance evidence.

## 1. Evidence reviewed

The review covered the Sprint 6 execution baseline and all eight accepted ADRs;
the stack-frame, SVC, PendSV, public-start, and S32K148 implementation records;
public and private headers; scheduler selection/current ownership, switch-plan,
yield, bootstrap, and start sources; Cortex C and assembly; S32K148 target,
startup, linker, smoke application, fault support, CMake, and post-link checks;
and the focused Sprint 6 tests.

`docs/architecture/sprint-5-acceptance-review.md`, named by the review request,
does not exist in the repository. The accepted Sprint 5 implementation records,
Sprint 4 acceptance review, and the actual Sprint 5 sources/tests were reviewed
instead. This documentation-history gap does not change executable ownership or
ABI, but the missing artifact should not be cited as repository evidence.

## 2. Functional-boundary result

PASS. Sprint 6 contains execution-port mechanism, initial launch, cooperative
ordinary switching, scheduler start, and target smoke support. It introduces no
tick advancement, SysTick enablement, runtime delay/wakeup, timer policy,
synchronization, dynamic post-start creation, FPU context, task deletion, EDF,
or RMS behavior.

The target NMI is a smoke-only Handler/MSP probe. It calls no scheduler API and
does not create an ISR-safe scheduler interface.

## 3. End-to-end execution ownership

| Stage | Owner | Accepted effect |
| --- | --- | --- |
| Reset/startup | S32K148 target | Hardware MSP, SRAM ECC writes, `.data`, `.bss`, VTOR, C entry |
| `rts_init()` | Portable bootstrap | Pool/queues/idle initialization and validated port setup |
| Task creation | Portable task transaction + port frame builder | Typed TCB registration and 64-byte initial frame |
| `rts_start()` | Portable scheduler | Select, READY→RUNNING, current publication, RUNNING lifecycle |
| First transfer | Cortex SVC | Consume startup handoff, restore R4–R11, select PSP, exception-return |
| Yield | Portable scheduler | Rotate equal-priority FIFO, select peer, freeze switch plan |
| Request | Cortex port | Set PENDSVSET with DSB/ISB |
| Ordinary transfer | Cortex PendSV | Save outgoing context, exchange saved SP, restore incoming context |
| Completion | Portable scheduler through C bridge | RUNNING→READY, READY→RUNNING, current publication, plan clear |

No layer crosses its approved queue, state, policy, or special-register boundary.

## 4. Initial frame and pointer encoding

PASS. `RTS_CM4F_INITIAL_FRAME_SIZE_BYTES` is 64 and the indexed layout is
R4–R11 followed by R0–R3, R12, LR, PC, xPSR. Saved SP points to R4. R0 receives
the argument; LR receives the private return trap; PC receives entry; xPSR is
only `0x01000000`. Every other word is zero. Guard tests prove successful setup
writes only the frame and failed setup does not modify the supplied region.

No EXC_RETURN, PRIMASK, CONTROL, FP register, FPSCR, or scheduler metadata is
stored in a task frame. Entry/trap function representations are copied into
`uint32_t` by character access inside the Cortex port. Portable code performs no
function-pointer conversion or object-pointer intermediate cast. Thumb-bit,
32-bit pointer, toolchain, and soft-float assumptions are checked.

## 5. SVC startup and activation window

PASS. The startup handoff contains first TCB, saved SP, cookie, and valid word.
Preparation requires coherent RUNNING/current state and no ordinary plan.
Consumption verifies identity and saved SP, then clears valid. The monotonic
attempt marker prevents duplicate startup.

The portable start commit occurs masked. The trigger executes `cpsie i; svc #0`
without an intervening instruction. No scheduler-aware interrupt is enabled in
that window. SVC immediately masks, consumes the dedicated handoff, restores
R4–R11 once, advances PSP to hardware R0, retains privileged Thread mode, sets
SPSEL, clears FPCA, executes ISB, unmasks, synthesizes `0xFFFFFFFD`, and returns
through the hardware basic frame. It does not select, mutate current, or touch
an ordinary switch plan. Successful embedded start cannot return.

## 6. Public start and rollback

PASS. RESET, RUNNING, ISR, and corrupt state are rejected. INITIALIZED selects
through the scheduler selector without FIFO rotation. Application FIFO head or
idle is adopted only through scheduler-owned current establishment. Lifecycle
commits immediately before the port handoff; no fake snapshot or PendSV request
is created.

A recoverable port return occurs before SVC. Rollback restores INITIALIZED,
RUNNING→READY, and null current while preserving queue order, pool, delay set,
and plan. Invalid Cortex preparation does not publish a handoff or set the
monotonic start marker; therefore a retry remains possible. Once a valid Cortex
handoff triggers SVC, there is no recovery path and none is promised.

## 7. PendSV request, priority, and handler

PASS. The request writes only ICSR.PENDSVSET, then DSB/ISB. Hardware pending is
idempotent and works while PRIMASK is set. There is no competing port-side
pending flag, selection, state transition, SysTick setup, or queue mutation.

The S32K148 contract declares four implemented bits, encodes SVC logical 13 as
`0xD0`, reserves logical 14/`0xE0` for SysTick, and forces PendSV logical
15/lowest as `0xF0`. Compile-time checks bind
target and port configuration; initialization programs and reads SHP back.
Task priorities are never used as exception priorities.

PendSV ordering matches the baseline:

1. mask and verify Thread/PSP/basic EXC_RETURN;
2. read PSP and save R4–R11 before any C call;
3. acquire the immutable snapshot on aligned MSP;
4. write outgoing saved SP through the symbolic TCB offset;
5. read incoming saved SP through that same offset;
6. restore incoming R4–R11 and publish hardware-frame PSP;
7. complete the exact snapshot through ordinary AAPCS C;
8. DSB/ISB, unmask, synthesize `0xFFFFFFFD`, and exception-return.

Only the TCB saved-SP field is accessed by assembly. Shared constants and C
`offsetof` assertions verify offset zero.

## 8. C bridge and snapshot safety

PASS. Incoming R4–R11 are restored before completion, but the bridge is ordinary
AAPCS C and therefore preserves those callee-saved registers. Handler C calls
run on 8-byte-aligned MSP; they do not change PSP. EXC_RETURN is loaded only
after the last call. Optimized `-O2` ARM compilation of the restricted bridge,
start, frame builder, switch planner, start, and yield units completed with all
warnings fatal and no FP instruction generation.

The handoff contains outgoing/incoming TCBs, the immutable generation snapshot,
and validated SP values. Acquire requires outgoing=current/RUNNING,
incoming=READY, distinct identities, valid bounds/alignment, and a PENDING plan.
It performs no selection or queue mutation. Completion requires the exact
handoff, generation, identities, ACTIVE plan, and SP values. Stale and duplicate
completion are rejected.

## 9. Switch completion and deferred reselection

PASS. Portable completion performs exactly outgoing RUNNING→READY, incoming
READY→RUNNING, `current_task=incoming`, and pending/active identity clear. It
does not rotate or relink queues, select, change priority/wait/delay/saved-SP,
call the port, or alter lifecycle.

`reselection_required` is deliberately not cleared by plan clear. A request
during ACTIVE records it, and completion preserves it for the later scheduler
evaluation. Focused bridge tests cover this behavior.

## 10. Spurious PendSV

PASS. The handler may copy outgoing R4–R11 below hardware PSP before discovering
that no PENDING plan exists, but it does not publish that temporary pointer or
change PSP/TCB/current. It restores the original exception LR and returns to the
same hardware frame. A no-plan result is accepted only when neither PENDING nor
ACTIVE exists; malformed ACTIVE acquisition remains fatal.

## 11. PSP/MSP, PRIMASK, IPSR, and STKALIGN

PASS by source/static contract; physical evidence remains pending.

- Reset and pre-start Thread mode use MSP; tasks use privileged Thread/PSP;
  SVC/PendSV and their bridges use Handler/MSP.
- Startup reserves an MSP region separate from statically placed kernel/task
  objects and linker-asserts no overlap.
- Critical entry reads exact PRIMASK, disables interrupts, and returns prior bit
  plus depth. Depth validates LIFO only; the captured bit alone determines
  restoration. Exit orders memory before unmask and executes ISB afterward.
- IPSR alone detects ISR context. PRIMASK has no role in context detection.
- Target initialization explicitly sets and reads CCR.STKALIGN. MSP and hardware
  frames are 8-byte aligned; public stacks, initial/saved PSP, and the 64-byte
  frame retain 16-byte alignment.
- Target-local NMI records Handler IPSR/MSP; tasks record PSP/MSP/CONTROL.

No architecture token or register type appears in the public API.

## 12. Vector, startup, linker, and no-FPU result

PASS for static integration. The vector table is exactly `0x400` bytes, flash
configuration is at `0x400`, and text begins at `0x410`. Strong Reset, NMI,
HardFault, SVC, PendSV, and default ownership is explicit. Runtime initialization
verifies VTOR plus SVC/PendSV slots, so vendor defaults cannot silently win.

Startup initializes the used S32K148 RAM window with aligned word writes for ECC,
then initializes C storage. SysTick remains default and disabled. The linker
reserves aligned MSP space and rejects static overlap.

The complete target policy is soft-float/no-FPU. Restricted GCC/Clang flags,
compile-time PCS checks, CPACR/FPCCR runtime configuration, basic EXC_RETURN,
and post-link VFP rejection agree. No heap symbol or host port is linked.

## 13. Smoke-test adequacy

PASS as infrastructure; NOT EXECUTED on physical hardware.

The application creates A then B at equal priority 2 with independent static
stacks and static arguments. Both increment distinct volatile counters and yield
forever. An AAPCS assembly helper loads different R4–R11 patterns, yields, checks
them after resume, and restores the caller's callee-saved registers. Task-local
checks cover argument identity, PSP/CONTROL, guards, and yield status. NMI proves
Handler/MSP. HardFault captures the hardware frame and SCB fault registers.

Idle non-execution is inferred from the accepted ready-set ordering and debugger
current-task inspection; no portable idle hook was added. GPIO was not added,
because debugger-visible counters plus register/stack/fault records provide two
independent observation classes without board-pin assumptions.

## 14. Verification record

The review reproduced or inspected:

- strict ARMv7E-M Thumb assembly for startup, NMI/HardFault, register test,
  SVC, and PendSV;
- strict ARM compilation of 21 target/portable C translation units using a
  temporary compile-only CMSIS surface;
- optimized `-O2` compilation of the restricted Cortex C bridge and portable
  start/switch/yield units;
- static ELF link with the production linker script;
- sections: vector `0x400`, flash config `0x400`, text `0x410`;
- exactly one Reset/SVC/PendSV/HardFault and both aligned task-stack symbols;
- SVC #0, PSP read/write, no VFP, no heap, and no host symbol;
- linked footprint: text 26,964 bytes, data 64 bytes, BSS 3,104 bytes.

Focused host test sources were compiled, but executable linking could not run in
this environment because the installed Windows Clang lacks the MSVC runtime
libraries `libcmt.lib` and `oldnames.lib`. CMake/CTest and the NXP device package
are also unavailable. This is toolchain-evidence incompleteness, not a passing
host-runtime claim.

## 15. Findings and corrections

No portable scheduler, initial-frame, SVC, PendSV, or ABI correctness defect was
found. Sprint 6.6 added the previously missing target/startup/linker/smoke layer.
During its integration checks, the freestanding runtime was corrected to provide
the compiler-emitted 8-byte EABI clear/copy helpers, unwind generation/sections
were made explicit, ARM attributes were retained for correct disassembly, and
yield failure was separated from R4–R11 corruption in smoke flags.

No accepted architecture decision was reopened. The reset-clock policy is a
minimal smoke-test target choice, not a portable-kernel change.

## 16. Acceptance limitations and Sprint 7 entry

The scheduler execution path is internally coherent and may be used as the
architecture baseline for Sprint 7 time-management design and host development.
Sprint 7 must not claim target timing or interrupt acceptance from this review.

Before physical Sprint 6 target acceptance is closed, integration must provide:

1. Debug and optimized CMake builds using the real NXP `S32K148.h` package;
2. successful post-link verification on those exact ELFs;
3. hardware evidence for reset→SVC→Task A, repeated A↔B PendSV switching,
   R4–R11 retention, PSP/MSP, priorities, STKALIGN, arguments, and guards; and
4. zero smoke/fault flags over a bounded run.

These are external validation gates, not unresolved scheduler architecture
alternatives.

## Sprint 6 Acceptance Decision

Sprint 6 ACCEPTED — execution architecture is coherent and ready for Sprint 7;
SDK-backed build and physical S32K148 smoke evidence remain mandatory before
target-hardware acceptance is claimed.
