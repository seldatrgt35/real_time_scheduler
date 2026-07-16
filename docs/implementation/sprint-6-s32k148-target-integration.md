# Sprint 6.6 — NXP S32K148 Target Integration

## Status and scope

The target integration, cooperative smoke image, startup/linker ownership, and
post-link checks are implemented. Physical S32K148 execution has not been
performed in this environment. A final build against the actual NXP S32K148
CMSIS device package also remains an integration-site verification because that
package is not installed in this workspace.

This Sprint 6 record describes the original cooperative image. Sprint 7A
supersedes only its SysTick and exception-priority statements; runtime delay,
time slicing, synchronization, and FP context remain outside Sprint 6.

## Target ownership and dependency

`targets/nxp/s32k148/` owns reset, the vector table, SRAM ECC initialization,
`.data`/`.bss` startup, VTOR, flash configuration, linker placement, PRIMASK,
IPSR detection, system-exception configuration, target fault capture, and
post-link checks. The portable kernel contains no NXP/CMSIS include. The generic
Cortex-M4F port contains no S32K peripheral driver.

Production compilation requires NXP's `S32K148.h` and its CMSIS core headers.
CMake requires `RTS_S32K148_DEVICE_INCLUDE_DIR` and rejects a directory without
that header. Target code uses CMSIS `SCB`, `FPU`, mask constants, and core
intrinsics; this repository defines no competing production SCB/NVIC layout.

NXP publishes S32K148 startup/linker sources under the S32 SDK
`platform/devices/S32K148/` tree. The memory layout follows NXP AN12218: P-Flash
at `0x00000000`, SRAM_L at `0x1FFE0000`, and SRAM_U at `0x20000000`. The default
standalone linker uses 1.5 MiB P-Flash and the SDK-compatible contiguous RAM
window ending at `0x2001F000`.

## Clock, startup, and linker policy

The first smoke image retains the valid 48 MHz FIRC reset/run clock and performs
no SCG/PLL switch. Sprint 7A uses that explicitly declared core clock for
SysTick; no peripheral clock is enabled.

Reset starts masked. `startup_s32k148.S` initializes all usable SRAM with aligned
32-bit writes before any RAM read, satisfying S32K148 ECC startup requirements.
It then copies `.data`, clears `.bss`, publishes VTOR, executes DSB/ISB, and
enters C. The C entry unmasks before `main()`; no scheduler-aware source is
enabled, satisfying the approved pre-start PRIMASK-clear contract.

The vector table has exactly 256 words. Core slots bind strong NMI, HardFault,
SVC, PendSV, and (as of Sprint 7A) SysTick handlers. Unused vectors use
`Default_Handler`. The linker asserts:

- vector size `0x400` at address zero;
- flash configuration at `0x400`;
- code begins at `0x410`;
- 8-byte MSP-top alignment; and
- no overlap with the reserved 4 KiB MSP region.

## Port initialization

`rts_port_initialize()` runs inside the bootstrap critical region. It requires
Thread mode on MSP, IPSR zero, PRIMASK set by the caller, aligned MSP, initial
CONTROL with MSP selected and FPCA clear, correct VTOR, exact vector ownership,
and inactive startup/switch handoffs.

It explicitly sets CCR.STKALIGN, removes CP10/CP11 access, disables ASPEN and
LSPEN, clears stale PendSV pending state, programs SVC/PendSV, executes DSB/ISB,
and reads every programmable contract back. A failure returns
`RTS_STATUS_PORT_ERROR` so `rts_init()` can roll back.

## PRIMASK, IPSR, priorities, and FPU

Critical entry captures the exact prior PRIMASK, disables configurable
interrupts, and returns an opaque token containing the prior bit and nesting
depth. Depth is used only to validate strict LIFO exit. Exit restores the exact
captured bit; it does not derive mask state from the nesting count.

`rts_port_is_in_isr()` is exactly `__get_IPSR() != 0`; PRIMASK is not used to
infer execution context.

The target declares four implemented priority bits:

| Exception | Logical | Encoded | Policy |
| --- | ---: | ---: | --- |
| SVC | 13 | `0xD0` | Startup service; higher urgency than SysTick |
| SysTick | 14 | `0xE0` | Kernel-aware periodic tick |
| PendSV | 15 | `0xF0` | Lowest configurable priority |

SysTick priority is programmed and read back by the Sprint 7A tick module. Task
priorities are unrelated. STKALIGN is set and read back; task stacks remain
16-byte aligned.

All target/kernel/port/smoke units use Cortex-M4 Thumb soft-float AAPCS and
no-FPU generation. GCC restricted C adds `-mgeneral-regs-only`; Clang adds
`-mfpu=none`. LTO is off. Initialization clears CP10/CP11 plus ASPEN/LSPEN and
requires FPCA clear. Post-link checks reject VFP mnemonics. Applications and
libraries must obey this same image-wide policy.

## Cooperative smoke application

`examples/s32k148_basic/` creates A then B, both at priority 2 with separate
1024-byte static stacks. FIFO startup selects A; explicit yield produces
A → B → A → B. Idle cannot be selected while either remains ready.

Each task receives a distinct static argument, records its identifier, PSP,
MSP, and CONTROL, increments a distinct volatile counter, checks its bottom
stack guard, and calls an AAPCS assembly register test. The assembly routine
saves the caller's R4–R11, loads a task-specific R4–R11 pattern, calls public
`rts_task_yield()`, validates all eight registers after resume, restores the
caller's registers, and returns. Normal task scheduling uses public APIs only.

Thread validation requires CONTROL.SPSEL set, FPCA clear, and nonzero
16-byte-aligned PSP. Each task pends NMI once. Strong `NMI_Handler` records its
pre-prologue MSP and IPSR; success requires exception number 2 and 8-byte MSP
alignment, proving the Handler/MSP path independently of scheduler internals.

Strong `HardFault_Handler` selects the stacked frame from EXC_RETURN and records
stacked R0–R3/R12/LR/PC/xPSR, CFSR, HFSR, MMFAR, BFAR, active MSP/PSP, and
EXC_RETURN before stopping. Assertions record and stop. This is bring-up support,
not a production diagnostic API. The approved private return trap is unchanged;
use a breakpoint on `rts_cm4f_task_return_trap` for the optional return test.

## Debugger-visible results

Inspect `g_rts_s32k148_smoke_record`:

- `failure_flags == 0`;
- both counters increase indefinitely and remain close;
- argument fields equal `0xA11A0001` and `0xB22B0002`;
- PSP values are nonzero and 16-byte aligned; and
- CONTROL has SPSEL set and FPCA clear.

Inspect `g_rts_s32k148_fault_record`:

- `assertion_failed == 0` and `hardfault_seen == 0`;
- `handler_probe_ipsr == 2`; and
- `handler_probe_msp` is nonzero and 8-byte aligned.

Failure bits cover init, both creates, unexpected start return, R4–R11,
Thread/Handler stack mode, arguments, guards, and yield. Idle non-execution is
verified from the ready-set contract and debugger inspection of current task;
no portable idle instrumentation hook was introduced.

## Build and artifacts

Use an ARM bare-metal CMake toolchain and the real NXP device include directory:

```sh
cmake -S . -B build-s32k148-debug \
  -DCMAKE_TOOLCHAIN_FILE=<arm-none-eabi-toolchain.cmake> \
  -DCMAKE_BUILD_TYPE=Debug \
  -DRTS_BUILD_HOST_TESTS=OFF \
  -DRTS_BUILD_S32K148_SMOKE=ON \
  -DRTS_S32K148_DEVICE_INCLUDE_DIR=<S32SDK-device-include>
cmake --build build-s32k148-debug --target rts_target_s32k148_smoke
```

Use `Release` for the optimized profile. Do not link hard-float SDK objects.
The build produces ELF, MAP, symbol table, disassembly, and section table files.
Post-build checks require one final Reset/SVC/PendSV/HardFault symbol, vectors,
stack symbols, PSP reads/writes, SVC #0, no host symbols, no heap, and no VFP.

## Flash and hardware procedure

1. Build with the real NXP package and flash `rts_s32k148_smoke.elf` using an
   S32K148-compatible probe.
2. Break at Reset, `rts_port_initialize`, SVC, task entry, PendSV, HardFault,
   and the task-return trap.
3. At SVC verify Handler/MSP; after return verify Task A argument and PSP.
4. At PendSV verify outgoing PSP publication, incoming SP load, and current-task
   completion A→B.
5. Run for a bounded interval and require both counters to advance with all
   application/fault flags zero and both guards intact.
6. Inspect SHP bytes `0xD0`/`0xE0`/`0xF0`, CCR.STKALIGN, CPACR, FPCCR, VTOR, and
   PendSV pending state.

## Verification performed without hardware

Performed on 2026-07-16:

- strict Clang ARMv7E-M/Thumb/soft-float compilation of 21 C units;
- strict assembly of startup, NMI/HardFault, register test, SVC, and PendSV;
- static ELF link with the production linker and a temporary compile-only CMSIS
  surface because the NXP package is unavailable;
- vector `0x400`, flash config `0x400`, text `0x410`;
- one final strong Reset, SVC, PendSV, and HardFault symbol;
- separate 16-byte-aligned task-stack symbols;
- SVC #0 and PSP read/write instructions; and
- no VFP, heap, or host-port symbols.

The temporary CMSIS surface was not committed and that ELF is not claimed as a
flashable acceptance artifact. Real SDK-backed CMake build, host CTest regression
(CMake is unavailable here), and physical execution remain pending.

## Remaining risks

- Real `S32K148.h` compatibility requires an integration-site build.
- The linker assumes a standalone full-flash image at zero and RAM ending at
  `0x2001F000`; bootloaders or different derivatives need reviewed overrides.
- Reset-clock retention is a smoke policy, not production clock configuration.
- Vector fetch, flash configuration, SRAM ECC behavior, timing, and repeated
  hardware switches remain physically unverified.
- PRIMASK retains its accepted worst-case interrupt-latency cost.

Sprint 6.6 implementation is ready for SDK-backed build and physical hardware
acceptance; hardware success is not claimed.
