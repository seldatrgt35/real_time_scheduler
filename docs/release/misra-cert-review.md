# MISRA-Oriented and CERT-Oriented Review

This is a rule-oriented engineering review, not a claim of MISRA compliance or
certification. No qualified compliance toolchain or safety process was used.

## Reviewed topics

- strict C11 effective type, aliasing, object lifetime, alignment, and incomplete handles;
- explicit-width integers and conversion warnings;
- wrap-safe half-range tick/deadline comparisons;
- macro argument safety and diagnostic macro side effects;
- no recursion or dynamic allocation in production paths;
- function-pointer placement in architecture frames;
- volatile target register access and assembly/C layout assertions;
- ISR/task context restrictions and PRIMASK mutation boundaries;
- assertions-disabled paths, bounded loops, ring generations, and queue ownership.

## Deviations and dispositions

| Topic | Disposition |
| --- | --- |
| Public enum-like values use fixed-width typedefs plus constants | Intentional ABI control; range validated at boundaries. |
| Opaque task/timer handles are incomplete pointers | Intentional identity; applications must not dereference. |
| Function pointer encoded in Cortex initial frame | Architecture ABI boundary, checked for representability and documented. |
| Unsigned tick/generation wrap | Intentional modular arithmetic with half-range ordering where order is required. |
| Public semaphore/mutex layouts expose opaque task pointers | Required caller-owned static storage; fields are not application-mutable. |
| Weak power/fatal hooks | Controlled link-time extension points with documented context restrictions. |
| Assembly handlers | Isolated port boundary with generated/verified constants and C static assertions. |
| PRIMASK global masking | Accepted single-core determinism tradeoff; latency analysis remains integration work. |

Generic compiler warnings and source scans found no release-blocking undefined
behavior. A certified project must repeat this review with its exact compiler,
linker, rule set, target, safety plan, and qualified analysis tools.
