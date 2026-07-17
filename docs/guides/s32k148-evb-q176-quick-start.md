# S32K148EVB-Q176 Quick Start

This procedure builds the repository's hardware smoke application with the
toolchain bundled in S32 Design Studio and loads it through the EVB's OpenSDA
debug interface. The repository remains a CMake project; it does not need to be
converted into an S32DS-managed source project.

## Prerequisites

- NXP S32K148EVB-Q176;
- S32 Design Studio with the ARM GCC toolchain and P&E debug support;
- S32 SDK S32K1xx containing `S32K148.h`;
- CMake 3.20 or newer available on `PATH`; and
- a data-capable USB cable connected to the board's OpenSDA USB connector.

The helper script auto-detects standard installations below `C:\NXP`. Custom
locations can be supplied with `-S32DSRoot` and `-SdkRoot`.

## Build the image

Open PowerShell in the repository root and run:

```powershell
.\tools\build_s32k148.ps1 -BuildType Debug -Policy FP -Clean
```

The build uses the S32DS `arm-none-eabi-gcc` compiler and produces:

```text
build-s32k148-fp-debug/targets/nxp/s32k148/rts_s32k148_smoke.elf
build-s32k148-fp-debug/targets/nxp/s32k148/rts_s32k148_smoke.hex
build-s32k148-fp-debug/targets/nxp/s32k148/rts_s32k148_smoke.srec
build-s32k148-fp-debug/targets/nxp/s32k148/rts_s32k148_smoke.map
```

Use the ELF for source-level debugging. HEX and SREC are provided for flash
programmers and production-style programming workflows.

## Connect the EVB

For the standard EVB-Q176 USB/OpenSDA setup, verify the board jumpers against
the board revision and NXP quick-start guide. Connect the OpenSDA USB port and
confirm that Windows detects the P&E/OpenSDA interface before starting S32DS.

## Load with S32 Design Studio

1. Open **Run > Debug Configurations**.
2. Create a **GDB PEMicro Interface Debugging** configuration.
3. Select the S32K148 device and the OpenSDA interface.
4. Browse to `rts_s32k148_smoke.elf` as the C/C++ application.
5. Enable download to flash, apply the configuration, and choose **Debug**.
6. Resume execution after the debugger stops at reset or `main`.

The scheduler starts through SVC. Ordinary task switches use PendSV. The smoke
application deliberately records results in RAM instead of depending on an LED
or UART driver.

## Observe the smoke test

Add this global object to the Expressions view:

```c
g_rts_s32k148_smoke_record
```

Expected observations after allowing the program to run are:

- `failure_flags == 0`;
- `task_a_count`, `task_b_count`, and `task_c_count` continue to increase;
- `observed_tick` continues to increase;
- task PSP values are nonzero and task CONTROL values select PSP;
- timer, semaphore, mutex, delay, and time-slice counters make progress; and
- `diagnostic_fatal_reason` and `diagnostic_invariant_failure` remain zero.

Also inspect `g_rts_s32k148_fault_record`; a valid run leaves its fault and
assertion indicators clear.

## Application entry points

- `examples/s32k148_basic/main.c` owns task stacks, descriptors, objects, and
  the calls to `rts_init()`, `rts_task_create()`, and `rts_start()`.
- `rts_smoke_task()` is the shared task entry used by the three descriptors.
- `kernel/` contains portable scheduler behavior.
- `ports/arm/cortex_m4f/` contains task-frame, SVC, and PendSV mechanics.
- `targets/nxp/s32k148/` contains reset, vectors, linker ownership, SysTick,
  LPTMR, interrupt priorities, and target diagnostics.

Start application experiments by making a copy of the smoke example or by
changing only its application-level task functions and descriptors. Do not put
application behavior into `kernel/`, the Cortex-M4F port, or the target layer.

