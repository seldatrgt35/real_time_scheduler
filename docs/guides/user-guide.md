# User Guide

Select one `rts_config.h`, set `RTS_CPU_COUNT` to one, select exactly one policy,
call `rts_init()`, register static tasks and timers, then call `rts_start()`.
Application task stacks must use static lifetime and 16-byte alignment; prefer
`RTS_TASK_STACK_DECLARE`. Semaphores and mutexes are caller-owned static objects
that must not move after initialization. Timer callbacks run serially in the
private service task and must not block.

FP uses the descriptor priority. RMS requires period and deadline, assigns
shorter periods higher ranks during startup creation, and then uses FP runtime
mechanics. EDF requires a relative deadline and recomputes an absolute deadline
at each release. The execution budget is metadata only; no admission control is
performed. See the policy guide and limitations before selecting EDF/RMS.

Task APIs are task-context-only unless explicitly named `_from_isr`. Task
creation and timer registration are INITIALIZED-only. Successful target start
does not return. All memory ownership and status behavior are frozen in the
Version 1 public API document.
