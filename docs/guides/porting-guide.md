# Cortex-M Porting Guide

Separate the reusable architecture port from the target layer. The architecture
port owns task-frame layout, stack direction/alignment, critical primitives,
ISR detection, SVC first launch, PendSV save/restore, PSP/MSP use, and exception
return. The target owns startup, vectors, clocks, exception priorities, the tick
and tickless timer, linker script, fatal evidence, and peripheral wake sources.

For a new Cortex-M target:

1. Define the no-FPU ABI policy and initial frame; keep saved SP at TCB offset zero.
2. Implement exact critical-token restoration and ISR/exception queries.
3. Bind one SVC and PendSV handler and verify their priorities and vector ownership.
4. Supply reset/startup and linker symbols without pulling a hosted C runtime or heap.
5. Implement periodic tick plus one-shot tickless sleep and elapsed-time measurement.
6. Build with CPU/Thumb/soft-float flags and reject VFP instructions.
7. Verify symbols, disassembly, PSP/MSP behavior, stack alignment, masks, and long-run switching on hardware.

The S32K148 files are an integration example, not portable kernel dependencies.
CMSIS/device headers must never be included by `kernel/`.
