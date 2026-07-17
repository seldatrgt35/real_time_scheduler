# Public API Compatibility Report

## Stable Version 1 headers

| Header | Stable inventory |
| --- | --- |
| `rts.h` | `rts_init`, `rts_start`, aggregate includes |
| `rts_task.h` | startup task creation, yield, relative delay |
| `rts_semaphore.h` | static semaphore type and task/ISR operations |
| `rts_mutex.h` | static non-recursive mutex and lock/unlock |
| `rts_timer.h` | opaque timer handle, configuration and control/query |
| `rts_power.h` | optional bounded sleep/resume application hooks |
| `rts_types.h` | statuses, ticks, priorities, task descriptor/handle, constants |
| `rts_version.h` | semantic version macros |

All scheduler, policy, ready/delay queue, TCB, switch-plan, CPU-local,
diagnostic, port, and target contracts remain private. No public CPU, affinity,
policy-object, current-task, or raw context API was added for SMP preparation.

The caller owns task stacks and public semaphore/mutex objects for their entire
useful lifetime. The kernel owns TCBs and timer objects. All public calls state
their lifecycle and execution-context restrictions in headers or the public API
guide. The task descriptor timing extension accepted in Sprint 12 is part of
the frozen v1.0.0 descriptor.
