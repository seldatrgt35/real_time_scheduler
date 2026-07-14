# ADR-0004: Version 1 No-FPU Policy

**Status:** Accepted for Version 1

## Decision

Scheduler tasks, kernel/port code, and scheduler-aware ISRs execute no floating-
point instructions. The complete image uses soft-float AAPCS; restricted GCC C
units use `-mgeneral-regs-only` and restricted Clang C units use `-mfpu=none`.
Hard-float or FP-enabled objects are incompatible and rejected.

## Consequences

Only the basic exception frame exists. Acceptance disassembly rejects VFP
instructions. FP support requires a later atomic frame/EXC_RETURN policy change.
