# Kernel Developer Guide

The scheduler core owns lifecycle, task states, CPU-local current-task state,
switch planning, blocking/wakeup orchestration, and reschedule notification.
The selected policy exclusively owns ready-node membership and ready ordering.
Delay, wait-object, mutex-owner, timer, callback, and trace membership each have
one owning module.

All portable shared-state mutation is performed under `rts_kernel_lock`; in
v1.0.0 it maps directly to nested PRIMASK critical sections. Production code
must not call heap functions, reinterpret public storage as private objects, or
modify intrusive nodes outside their owner. Every traversal must be bounded by
a configured capacity. New public surface requires an API compatibility review;
new assembly-visible fields require an ADR and offset verification.

Run the complete host suite, all policy profiles, strict configuration matrix,
`tools/release_audit.py`, ARM syntax/ABI checks, and target link checks for every
release candidate.
