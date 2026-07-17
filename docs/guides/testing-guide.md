# Testing and Verification Guide

The default CMake build compiles deterministic host tests with strict warnings.
CTest covers intrusive containers, task lifecycle, scheduling/switch planning,
delay/time slicing, synchronization/inheritance, timers, diagnostics, power,
FP/RMS/EDF, CPU-local boundaries, and deterministic 50,000-event policy stress
profiles. Release/no-slicing/no-tickless and quantum variants exercise feature
specialization.

`tools/release_audit.py` checks policy leakage, heap calls, public/private include
boundaries, handler ownership, configuration selection, release version, legacy
reschedule symbols, and saved-SP offset assertions. Cortex builds additionally
run target symbol/disassembly checks for handler uniqueness, heap/host symbols,
PSP access, SVC, and VFP instructions.

Hardware results must record board revision, toolchain, commit, policy/config,
clock, run duration, counters, fatal record, stack margins, and raw benchmark
samples. Absence of board evidence must be reported as NOT RUN, never PASS.
