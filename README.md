# Real-Time Scheduler v1.0.0

A clean, statically allocated real-time scheduler designed from first principles for single-core embedded systems. The project targets the ARM Cortex-M4F architecture and NXP S32K148 while keeping the portable kernel independent of vendor SDKs and device-specific code.

This is an original scheduler design—not a FreeRTOS, Zephyr, RTX, or ThreadX port or clone. Its architecture emphasizes deterministic behavior, explicit ownership, strict C11 conformance, narrow module contracts, and code that remains understandable enough for design review and education.

> **Project status:** v1.0.0 software release candidate. Compile-time FP, RMS, and EDF scheduling, delay/time slicing, synchronization, software timers, diagnostics, tickless idle, Cortex-M4F, and S32K148 integration are implemented. Final acceptance is conditional because physical S32K148 release evidence is not available. The project is not safety-certified.

## Design goals

- Deterministic execution with statically bounded time and memory usage
- Static allocation only; no heap and no task deletion in Version 1
- Compile-time-selected FP, RMS, or EDF scheduling with no runtime policy switch
- FIFO ordering within each priority and optional round-robin rotation
- A portable C11 kernel separated from architecture and board support
- Caller-owned, byte-counted task stacks with a kernel-owned private TCB pool
- Opaque public task handles and no public exposure of kernel object layouts
- Explicit lifecycle, error handling, ownership, and invariant contracts
- A host port for deterministic unit testing without executing tasks

## Current implementation

The repository currently includes:

- intrusive doubly linked lists;
- one FIFO ready queue per priority plus a ready-priority bitmap;
- an absolute-tick delay queue with wrap-safe ordering;
- a compile-time-sized application task pool and a separate idle task;
- task descriptor validation and private TCB initialization;
- transactional, startup-only task creation;
- kernel bootstrap and idle-task registration;
- highest-ready selection and scheduler-owned `current_task` management;
- immutable switch planning and public task-yield integration;
- public relative task delay, ordered wakeup, and higher-priority preemption;
- a portable wrap-safe tick core and S32K148 SysTick source;
- compile-time optional tick-driven equal-priority round robin;
- statically owned counting/binary semaphores with deterministic priority/FIFO waiters;
- finite/forever semaphore waits, direct handoff, timeout arbitration, and ISR-safe give;
- non-recursive mutexes with bounded transitive priority inheritance, timeout,
  direct handoff, and deterministic restoration;
- centralized fatal/assert handling and S32K148 HardFault evidence capture;
- optional stack guards, stack watermarking, runtime counters, private
  snapshots, a fixed trace ring, and bounded global invariant validation;
- statically pooled one-shot and periodic software timers with a dedicated
  active queue, bounded callback FIFO, and private deferred service task;
- a portable tickless-idle power manager with wrap-safe earliest-wake
  calculation, coalesced time compensation, optional hooks, and diagnostics;
- a policy-independent scheduler core with FP bitmap/FIFO, RMS static period
  ranking, and an ordered EDF ready-set plugin;
- deterministic host sleep simulation and an S32K148 LPTMR0/WFI target path;
- deterministic 20,000-event diagnostics stress tests in enabled, release, and
  no-time-slicing configurations;
- the public scheduler-start transaction;
- a deterministic host port and focused host tests;
- a Cortex-M4F 64-byte initial task frame;
- SVC-based first-task startup; and
- PendSV-based ordinary context switching using PSP for tasks and MSP for exceptions.

The following Version 1 work remains:

- physical S32K148 validation of the implemented startup, vector, linker, and timed smoke image;
- target execution tests and hardware validation.

Runtime task creation, task deletion, admission control, runtime policy
switching, and floating-point context switching remain outside the baseline.

## Scheduling model

Version 1 selects exactly one scheduling policy at compile time. FP retains the
accepted larger-number-wins FIFO model; RMS derives those priorities from task
periods; EDF orders tasks by absolute deadline and release FIFO order.

- priorities range from `0` to `RTS_PRIORITY_COUNT - 1`;
- priority `0` is reserved for the kernel idle task;
- application priorities start at `1`;
- larger numeric values have higher scheduling priority;
- tasks at the same priority are ordered FIFO;
- yielding rotates only the running task's own priority queue; and
- the running task remains linked at the head of its ready queue.

The idle task is permanently available as the lowest-priority fallback and does not consume one of the `RTS_MAX_TASKS` application slots.

## Public API

The intentionally small Version 1 API is declared under `include/rts/`:

```c
rts_status_t rts_init(void);
rts_status_t rts_start(void);

rts_status_t rts_task_create(
    const rts_task_config_t *config,
    rts_task_handle_t *out_handle);

rts_status_t rts_task_yield(void);
rts_status_t rts_task_delay(rts_tick_t delay);

rts_status_t rts_semaphore_init(rts_semaphore_t *semaphore,
                                rts_count_t initial_count,
                                rts_count_t maximum_count);
rts_status_t rts_semaphore_take(rts_semaphore_t *semaphore,
                                rts_tick_t timeout);
rts_status_t rts_semaphore_give(rts_semaphore_t *semaphore);
rts_status_t rts_semaphore_give_from_isr(
    rts_semaphore_t *semaphore,
    bool *higher_priority_task_woken);

rts_status_t rts_mutex_init(rts_mutex_t *mutex);
rts_status_t rts_mutex_lock(rts_mutex_t *mutex, rts_tick_t timeout);
rts_status_t rts_mutex_unlock(rts_mutex_t *mutex);

rts_status_t rts_timer_init(const rts_timer_config_t *config,
                            rts_timer_handle_t *out_handle);
rts_status_t rts_timer_start(rts_timer_handle_t timer);
rts_status_t rts_timer_stop(rts_timer_handle_t timer);
rts_status_t rts_timer_restart(rts_timer_handle_t timer);
bool rts_timer_is_running(rts_timer_handle_t timer);
```

Application tasks are registered only after `rts_init()` and before `rts_start()`. A successful embedded start transfers execution to the selected task and normally does not return.

Semaphore objects are caller-owned, zero-initialized static objects. An initialized semaphore must remain at one stable address and must not be copied.

## Memory and ownership model

All memory is statically bounded:

- the kernel owns `RTS_MAX_TASKS` private application TCB objects;
- the kernel owns `RTS_MAX_TIMERS` private software-timer objects, their
  dedicated ordered queue, and the bounded callback-work ring;
- the kernel owns separate private idle and timer-service TCBs and stacks;
- the application owns every application-task stack for the task's lifetime;
- a successful opaque task handle points directly to a stable private TCB; and
- no dynamic allocation, pool growth, or task deletion is used.

Use `RTS_TASK_STACK_DECLARE` to create a statically allocated stack with the required 16-byte public alignment:

```c
#include "rts/rts.h"
#include "rts/rts_task.h"

RTS_TASK_STACK_DECLARE(worker_stack, 512u);

static void worker(void *argument)
{
    (void)argument;

    for (;;)
    {
        /* Application work. */
        (void)rts_task_yield();
    }
}

int main(void)
{
    rts_task_handle_t worker_handle = NULL;
    const rts_task_config_t worker_config = {
        .entry = worker,
        .argument = NULL,
        .stack_buffer = worker_stack,
        .stack_size_bytes = sizeof worker_stack,
        .priority = 2u
    };

    if (rts_init() != RTS_STATUS_OK)
    {
        return 1;
    }

    if (rts_task_create(&worker_config, &worker_handle) != RTS_STATUS_OK)
    {
        return 2;
    }

    if (rts_start() != RTS_STATUS_OK)
    {
        return 3;
    }

    return 4; /* Unreachable after a successful embedded start. */
}
```

The repository supplies the standalone smoke startup/linker configuration. A
production board integration must still supply its reviewed clock, flashing,
and deployment policy.

## Configuration

Every build selects exactly one `rts_config.h`. The current private and public contracts require these macros:

| Macro | Purpose |
| --- | --- |
| `RTS_MAX_TASKS` | Number of application-task pool slots |
| `RTS_CPU_COUNT` | Must equal one in v1.0.0; values above one are rejected |
| `RTS_MAX_TIMERS` | Number of private software-timer pool slots |
| `RTS_TIMER_SERVICE_PRIORITY` | Fixed priority of the private callback service |
| `RTS_TIMER_SERVICE_STACK_SIZE_BYTES` | Private callback-service stack bytes |
| `RTS_TIMER_CALLBACK_QUEUE_CAPACITY` | Fixed callback-work ring capacity |
| `RTS_PRIORITY_COUNT` | Total priority levels, including idle priority zero |
| `RTS_TICK_RATE_HZ` | Scheduler tick frequency |
| `RTS_POLICY_FIXED_PRIORITY` | Selects the fixed-priority plugin (exactly one policy is `1`) |
| `RTS_POLICY_RMS` | Selects static rate-monotonic period ranking |
| `RTS_POLICY_EDF` | Selects absolute-deadline ready ordering |
| `RTS_ENABLE_TIME_SLICING` | Enables equal-priority time slicing |
| `RTS_TIME_SLICE_TICKS` | Length of one time slice |
| `RTS_IDLE_STACK_SIZE_BYTES` | Size of the private idle-task stack |
| `RTS_ENABLE_ASSERTIONS` | Enables internal contract assertions |
| `RTS_ENABLE_DIAGNOSTICS` | Enables the private diagnostic snapshot layer |
| `RTS_ENABLE_TRACE` | Enables the fixed overwrite-oldest trace ring |
| `RTS_ENABLE_STACK_GUARDS` | Reserves and verifies the low stack guard |
| `RTS_ENABLE_STACK_WATERMARK` | Enables on-demand pattern-based high-water scans |
| `RTS_ENABLE_RUNTIME_STATS` | Adds bounded kernel and per-task counters |
| `RTS_ENABLE_INVARIANT_CHECKS` | Enables bounded full-kernel validators |
| `RTS_TRACE_CAPACITY` | Compile-time trace-entry capacity |
| `RTS_STACK_GUARD_SIZE_BYTES` | Low-address guard reservation per stack |

The host tests provide example configurations under `tests/config*`.

## Build and run the host tests

Requirements:

- CMake 3.20 or newer
- a C11 compiler (GCC, Clang, or MSVC)
- CTest, distributed with CMake

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Warnings are treated as errors. The host port validates kernel transactions and architecture handoffs deterministically; it does not execute scheduler tasks or emulate Cortex-M exception return.

## Cortex-M4F port

The Cortex-M4F port is selected explicitly and must be built with a suitable cross-toolchain. It uses the soft-float ABI and intentionally does not save floating-point context.

```sh
cmake -S . -B build-cortex-m4f \
  -DCMAKE_TOOLCHAIN_FILE=<arm-toolchain.cmake> \
  -DRTS_BUILD_HOST_TESTS=OFF \
  -DRTS_BUILD_CORTEX_M4F_PORT=ON \
  -DRTS_CONFIG_INCLUDE_DIR=<directory-containing-rts_config.h> \
  -DRTS_CORTEX_M_NVIC_PRIORITY_BITS=4 \
  -DRTS_CORTEX_M_PENDSV_PRIORITY=15 \
  -DRTS_CORTEX_M_SVC_PRIORITY=13

cmake --build build-cortex-m4f
```

The priority values above are an example and must match the final MCU integration and approved interrupt-priority policy. An SDK-dependent cooperative S32K148 smoke image is available with `RTS_BUILD_S32K148_SMOKE=ON`; provide NXP's `S32K148.h` directory through `RTS_S32K148_DEVICE_INCLUDE_DIR`.

For the NXP S32K148EVB-Q176 with S32 Design Studio on Windows, use the
[board quick-start guide](docs/guides/s32k148-evb-q176-quick-start.md). A helper
script builds a flashable ELF, Intel HEX, and Motorola S-record with the S32DS
ARM GCC toolchain:

```powershell
.\tools\build_s32k148.ps1 -BuildType Debug -Policy FP -Clean
```

## Repository layout

```text
include/rts/                 Public API and public types
kernel/                      Portable kernel and private contracts
ports/host/                  Deterministic host-test port
ports/arm/cortex_m4f/        Cortex-M4F stack, SVC, and PendSV port
tests/unit/                  Focused host unit and contract tests
tests/config*/               Test configuration variants
docs/architecture/           Baselines, ADRs, and acceptance reviews
docs/implementation/         Sprint implementation records
docs/guides/                 User, kernel, policy, porting, and test guides
docs/release/                Release notes, limits, compatibility, and evidence
tools/                       Reproducible release and private-layout audits
.github/workflows/           Host, ARM syntax, documentation, and static CI
```

## Architecture documentation

The code is developed against reviewed architecture contracts. Useful starting points are:

- [Sprint 0 architecture baseline](docs/architecture/sprint-0-baseline.md)
- [Version 1 public API](docs/architecture/version-1-public-api.md)
- [Private kernel baseline](docs/architecture/sprint-2-internal-kernel.md)
- [Sprint 4 acceptance review](docs/architecture/sprint-4-acceptance-review.md)
- [Cortex-M4F execution contract](docs/architecture/sprint-6-cortex-m4f-execution-contract.md)
- [Sprint 6 acceptance review](docs/architecture/sprint-6-acceptance-review.md)
- [Sprint 9 diagnostics implementation](docs/implementation/sprint-9-kernel-diagnostics.md)
- [Sprint 9 acceptance review](docs/architecture/sprint-9-acceptance-review.md)
- [Sprint 10A timer infrastructure](docs/implementation/sprint-10a-timer-infrastructure.md)
- [Sprint 10B callback service](docs/implementation/sprint-10b-timer-callback-service.md)
- [Sprint 10 acceptance review](docs/architecture/sprint-10-acceptance-review.md)
- [Sprint 11 tickless idle and power architecture](docs/implementation/sprint-11-tickless-idle.md)
- [Sprint 12 scheduler policy framework](docs/implementation/sprint-12-policy-framework.md)
- [SMP preparation boundary](docs/architecture/smp-preparation.md)
- [Sprint 13 final acceptance review](docs/architecture/sprint-13-final-acceptance-review.md)
- [User guide](docs/guides/user-guide.md)
- [Kernel developer guide](docs/guides/kernel-developer-guide.md)
- [Porting guide](docs/guides/porting-guide.md)
- [Policy guide](docs/guides/policy-guide.md)
- [Testing guide](docs/guides/testing-guide.md)
- [S32K148 target integration guide](docs/guides/target-integration-guide.md)
- [v1.0.0 benchmark and memory report](docs/release/benchmark-report.md)
- [v1.0.0 public API compatibility](docs/release/public-api-compatibility.md)
- [v1.0.0 release manifest](docs/release/release-manifest-v1.0.0.md)
- [v1.0.0 known limitations](docs/release/known-limitations.md)
- [v1.0.0 release notes](docs/release/release-notes-v1.0.0.md)

The ADRs under `docs/architecture/adr/` record key ABI, interrupt, stack-frame, and context-switch decisions.

## Engineering constraints

- C11 with strict object-access and aliasing rules
- single-core execution
- no heap use in the kernel
- no exposed TCB layout
- no hard dependency on CMSIS or the NXP SDK in the portable kernel
- no FPU context support in the current Cortex-M4F execution contract
- assertions for internal corruption; public status codes for recoverable API errors

## Release evidence still required

1. Run the FP/RMS/EDF S32K148 hardware matrix and archive debugger/trace evidence.
2. Run the bounded long-duration target profile and fault-capture checks.
3. Measure target stack margins, context-switch latency, and PRIMASK windows.
4. Complete product-specific safety, timing, and tool qualification activities.

Third-party tool and SDK notices are recorded in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
