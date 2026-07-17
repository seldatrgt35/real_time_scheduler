# NXP S32K148 Target Integration Guide

The target layer supplies reset/vector/linker ownership, clock assumptions,
exception priorities, SysTick, LPTMR tickless wake, PRIMASK/IPSR access, fatal
capture, and smoke observability. Configure the image with
`RTS_BUILD_S32K148_SMOKE=ON`, the NXP device-header directory, a reviewed ARM
toolchain file, and `RTS_S32K148_POLICY=FP|RMS|EDF`.

The final image verifier requires exactly one Reset, SVC, PendSV, SysTick,
LPTMR0, and HardFault handler; forbids heap/host symbols and VFP instructions;
and confirms PSP/SVC instructions. The NXP SDK/device package is not vendored or
redistributed. Physical validation remains mandatory before production use.
