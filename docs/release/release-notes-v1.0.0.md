# Real-Time Scheduler v1.0.0 Release Notes

This is the first stable API release of the statically allocated single-core
Real-Time Scheduler. It includes compile-time FP, RMS, and EDF policies; task
delay and time slicing; semaphores and non-recursive priority-inheritance
mutexes; software timers; tickless idle; diagnostics; a host test port; a
Cortex-M4F SVC/PendSV port; and S32K148 integration.

## Compatibility policy

Headers under `include/rts/` are the stable Version 1 source/API surface.
Function signatures, public object layouts, status values, task descriptor,
opaque handle rules, static ownership, and lifecycle behavior are frozen for
the v1.x line. Compatible releases may add functions or enum values at the end;
they will not silently change existing semantics or public object layouts.
Private headers, TCB layout beyond the saved-SP assembly contract, and internal
module functions are not ABI commitments.

The release version is available through `rts/rts_version.h` as numeric macros,
an encoded value, and `RTS_VERSION_STRING`.

## Release decision

Software architecture, host verification, policy builds, and ARM compile/ABI
checks form the release candidate. Physical S32K148 execution evidence is not
available in the current environment, so final acceptance remains conditional.
See the final acceptance review and known limitations for exact evidence.
