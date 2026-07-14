# ADR-0001: Cortex-M4F PSP/MSP Execution Model

**Status:** Accepted for Version 1

## Decision

Tasks run privileged in Thread mode on PSP. Exceptions and every C bridge called
from an exception run on MSP. Reset and scheduler startup begin on MSP. SVC
performs first-task entry; PendSV performs ordinary switches.

## Consequences

The Cortex port exclusively owns PSP, MSP, CONTROL, and exception machinery.
Target startup must initialize and align MSP; portable kernel code remains
architecture-neutral.
