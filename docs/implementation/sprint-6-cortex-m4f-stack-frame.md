# Sprint 6.2 — Cortex-M4F Initial Stack Frame

## Scope and source ownership

`ports/arm/cortex_m4f/port_stack.c` implements only the Cortex-M4F stack-size
queries, initial stack construction, function-pointer encoding, and private
task-return trap. It does not read or mutate a TCB, pool, queue, lifecycle,
scheduler plan, interrupt controller, or special register. SVC, PendSV, startup,
and task execution remain absent.

## Frame and saved-SP calculation

The initializer validates every input before writing. It converts the supplied
base to `uintptr_t`, rejects `base + size` overflow, calculates the numeric end,
aligns that value downward to 16 bytes, proves that 64 bytes remain within the
region, and only then converts the validated frame address back to a character
pointer. The saved SP equals `aligned_top - 64`.

Both base and size must honor the public 16-byte contract. The resulting saved
SP is consequently 16-byte aligned and also satisfies Cortex-M's 8-byte
exception requirement. The structural minimum is exactly 64 bytes and size
granularity is exactly 16 bytes. Applications remain responsible for additional
runtime stack depth and safety margin.

The frame is exactly:

| Word | Offset | Register | Value |
|---:|---:|---|---|
| 0–7 | `0x00–0x1C` | R4–R11 | zero |
| 8 | `0x20` | R0 | task argument address |
| 9–11 | `0x24–0x2C` | R1–R3 | zero |
| 12 | `0x30` | R12 | zero |
| 13 | `0x34` | LR | private return trap encoding |
| 14 | `0x38` | PC | task entry encoding |
| 15 | `0x3C` | xPSR | `RTS_CM4F_INITIAL_XPSR` (`0x01000000`) |

There is no EXC_RETURN, FP register, FPSCR, CONTROL, mask state, or task
metadata word.

## Strict-C representation handling

Caller memory remains an array of bytes; no structure or `uint32_t` lvalue is
overlaid on it. Frame words are emitted by an explicit little-endian four-byte
writer. This avoids alignment, effective-type, packing, and strict-aliasing
dependencies.

Entry and return-trap function pointers are never converted through object
pointers. The Cortex-private code copies each function pointer's four-byte
object representation through `unsigned char` into a `uint32_t`, as approved for
the GCC/Clang ARM EABI contract. Compile-time checks require four-byte function
pointers, and runtime construction verifies the Thumb encoding bit before any
stack write. An unrepresentable argument or invalid function representation
returns `RTS_STATUS_PORT_ERROR` with a null saved SP.

Null entry returns `RTS_STATUS_INVALID_TASK_CONFIG`. Invalid/null stack regions,
alignment, granularity, minimum size, overflow, or bounds return
`RTS_STATUS_INVALID_STACK`. All failure paths leave stack bytes untouched.

## FPU and build policy

The initializer creates only the ARM basic frame. Cortex port objects use
`-mcpu=cortex-m4 -mthumb -mfloat-abi=soft`; GCC adds
`-mgeneral-regs-only`, while Clang adds `-mfpu=none`. Cortex-private headers
reject hard-float PCS and enabled FP code generation.

Cross-build example:

```text
cmake -S . -B build-cm4 \
  -DRTS_BUILD_HOST_TESTS=OFF \
  -DRTS_BUILD_CORTEX_M4F_PORT=ON \
  -DRTS_CONFIG_INCLUDE_DIR=<application-config-directory> \
  -DRTS_CORTEX_M_NVIC_PRIORITY_BITS=4 \
  -DRTS_CORTEX_M_PENDSV_PRIORITY=15 \
  -DRTS_CORTEX_M_SVC_PRIORITY=14
cmake --build build-cm4
```

The selected image must link exactly one port. The Cortex target includes
`port_stack.c`; no host-port source is part of that target.

## Verification

`tests/unit/test_cortex_m4f_stack_frame.c` is a Cortex-compiled contract test
with byte-wise decoding. When executed on a future emulator or target it checks
the exact 16 words, R0 argument, entry/trap representations, Thumb xPSR,
minimum/granularity failures, misalignment, overflow, untouched guard bytes,
null inputs, failure atomicity, alignment, bounds, and deterministic repeated
construction. It intentionally uses no packed overlay or library allocator.

Sprint 6.2 validation compiles production and test objects with strict warnings
for `arm-none-eabi`, Cortex-M4, Thumb, soft-float, and no-FPU. Object inspection:

```text
llvm-objdump -d port_stack.o
llvm-nm port_stack.o
```

Acceptance rejects VFP mnemonics and heap calls, verifies only the three stack
contract symbols plus the private trap are defined, verifies no SVC/PendSV
handler symbol, and confirms constants 64, 16, and `0x01000000` in generated
code. The task-creation sources and this port object are also combined in an ARM
relocatable link to prove that `rts_task_create()` resolves the selected
`rts_port_stack_initialize()` without linking the host port.

Hardware or emulator execution is still required to turn the compiled contract
test into runtime Cortex evidence. SVC/PendSV consumption of the initialized
frame, actual PSP restoration, and first-task execution remain Sprint 6.3–6.5
dependencies.
