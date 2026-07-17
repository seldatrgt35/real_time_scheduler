# Known Limitations — v1.0.0

- Single-core only. `RTS_CPU_COUNT` must equal one; SMP preparation is not SMP support.
- Static allocation only. Application tasks are created before `rts_start()`;
  there is no runtime creation, deletion, pool growth, or heap use.
- Cortex-M4F tasks use the soft-float ABI. Floating-point context, MPU isolation,
  privilege separation, and user mode are not implemented.
- PRIMASK protects portable kernel mutations. Long task creation, queue scans,
  inheritance chains, timer expiry batches, and tickless transitions contribute
  to worst-case interrupt latency and require application-specific analysis.
- RMS assigns ranks during startup task creation and performs no admission test.
- EDF performs no utilization, demand-bound, or overload admission analysis and
  therefore provides ordering, not feasibility guarantees.
- EDF does not implement deadline inheritance. Mutex waiters and classic
  priority inheritance retain effective fixed-priority semantics.
- The timer-service task is deliberately urgent: reserved high priority under
  FP/RMS and a one-tick relative deadline under EDF. Application callbacks must
  be bounded and nonblocking; callback WCET is the application's responsibility.
- No networking, filesystem, DVFS, dynamic clock scaling, or partitioned scheduling.
- The code has received a MISRA-oriented and CERT-oriented review, but is not
  certified MISRA compliant and has not completed a functional-safety lifecycle.
- Physical S32K148 validation, long-run evidence, cycle benchmarks, stack
  high-water measurements, and target memory-map evidence require board access.
  Host and ARM compile evidence cannot substitute for those measurements.
- Supported source language is strict C11. Verified repository profiles use
  Clang host and ARM Clang in this environment; GCC/ARM GCC CI jobs are defined
  but require those external toolchains.
