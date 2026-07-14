# Real-Time Scheduler — Sprint 0 Architecture Baseline

**Target:** ARM Cortex-M4F / NXP S32K148, single core, C11, static allocation, CMake  
**Document status:** Proposed for approval  
**Scope:** Architecture through the first working scheduler milestone; no implementation is specified here.

> **Storage amendment:** The original caller-owned opaque TCB-storage decision was superseded after a C11 effective-type review. Version 1 uses a kernel-owned typed pool of `RTS_MAX_TASKS` application TCBs plus a separate idle TCB; application stacks remain caller-owned. See [Version 1 Public API Design](version-1-public-api.md#architecture-amendment-kernel-owned-static-tcb-pool).

## 1. Critical review of the draft architecture

| Draft issue | Why it matters | Correction |
|---|---|---|
| `scheduler_core.c` and `dispatcher.c` divide one state transition across two owners. | A forwarding dispatcher obscures the scheduling path and makes current-task ownership ambiguous. | The scheduler core owns scheduling points, current-task selection, and switch requests; there is no dispatcher module. |
| Round robin was shown as a sibling policy to fixed priority. | That duplicates ready-task selection and can produce conflicting definitions of priority versus fairness. | Fixed priority is the only initial policy; round robin is optional rotation of the FIFO queue at the running priority. |
| `critical.h` was public. | Application-held kernel critical sections can break latency bounds and kernel invariants. | Kernel interrupt masking is private to the port contract. Public scheduler locking is deferred unless a demonstrated application requirement exists. |
| Public `time.h` implied a general time service. | It risks freezing tick width, tick frequency, wrap behavior, and timer implementation into the ABI. | Version 1 exposes relative delay in scheduler ticks through `rts_task.h`; it exposes no clock, absolute time, timer, or conversion API. |
| `port/`, `platform/`, and an implied board layer lacked exact boundaries. | Clock, timer, startup, and exception ownership can be duplicated or coupled to an SDK. | Use a Cortex-M4F architecture port and an S32K148 target integration. Add no board layer until multiple boards have distinct pin/clock/peripheral composition. |
| Global `scheduler_config.h` and `platform_config.h` could be included from anywhere. | Configuration can become an uncontrolled dependency channel from target code into the portable kernel. | The application supplies one generated or selected public configuration header; kernel and port validate and derive private constants in their own headers. |
| A broad policy interface anticipated EDF operations and runtime selection. | Function pointers and generic attributes increase paths and WCET without serving the first policy. | Compile-time composition uses a narrow private ready-set contract implemented by fixed priority; document the future seam without a runtime policy object. |
| Task manager, task-state module, and dispatcher overlapped state-transition ownership. | Distributed TCB mutation makes invariants hard to audit. | `task.c` initializes task objects; `scheduler.c` owns runtime task-state transitions; `ready_queue.c` and `delay_queue.c` exclusively own their links. |
| Future synchronization and policy directories were present but empty. | Placeholder production structure implies unsupported contracts and adds maintenance noise. | Do not create directories or source files until the feature enters an approved sprint. |
| A diagnostics subsystem was proposed before defining mandatory checks. | Optional instrumentation can accidentally affect timing or correctness. | Compile-time kernel assertions are the only baseline diagnostic. Optional trace and statistics are deferred. |
| `application/` was inside the reusable library structure. | Product code and scheduler releases then acquire coupled lifecycles. | Keep demonstrations under `examples/`; a real product application consumes the scheduler as a separate CMake component. |
| The idle task appeared as its own module. | Its small lifecycle is inseparable from scheduler initialization and adds an unnecessary interface. | The scheduler core owns the statically configured idle task; its entry function may live in `scheduler.c`. |
| Generic direct module calls were proposed without queue ownership rules. | Intrusive nodes can be linked twice or mutated by multiple subsystems, corrupting kernel state. | Each intrusive link has one owning data-structure module; state changes occur only through scheduler-core orchestration. |
| Tick processing could imply scanning every task in the ISR. | Work proportional to configured task count creates avoidable interrupt jitter. | The delay queue is ordered by wake time; tick handling examines only the head and tasks expiring at that tick. The number of simultaneous expirations is documented as the remaining bounded-by-task-count case. |
| Host testing was mentioned but the architecture port contract included target-only exception behavior. | Portable logic cannot be unit tested if its link seam requires CMSIS or exception symbols. | The kernel calls a minimal private port contract; a host port implements the same contract without CMSIS or NXP headers. |

## 2. Resolved architectural decisions

### 2.1 Dispatcher ownership

There is no separate dispatcher module.

- **Scheduling decision:** `kernel/scheduler.c` detects a scheduling point and asks the ready-set owner for the highest eligible task.
- **Reschedule request:** the scheduler core records that selection must be reconsidered; ISR-safe internal entry points may set this state.
- **Context-switch request:** the scheduler core calls the private port operation that pends or emulates a switch.
- **Context save and restore:** exclusively the Cortex-M4F port, specifically PendSV and its assembly support.
- **First-task startup:** the scheduler core selects the first task; the Cortex-M4F port starts it through SVC and exception return.

The scheduler owns `current_task`. The port may read or exchange saved stack-pointer fields only through the deliberately narrow port/kernel contract; it never chooses a task.

### 2.2 Round-robin ownership

Round robin is a ready-queue rotation mechanism inside the fixed-priority ready-set implementation. Each priority has one FIFO queue. The scheduler tick accounts for the current task's optional quantum and asks the ready set to rotate that priority only when another task at the same priority is ready. Fixed-priority selection remains single-sourced in `ready_queue.c`.

### 2.3 Critical sections

Kernel critical sections are private. The portable kernel uses `port_critical_enter()` and `port_critical_exit()` through `port.h`; the token returned by entry preserves the previous mask state and supports validated nesting. Application code cannot call these operations.

Scheduler locking is a distinct dispatch-control concept and is not part of the version 1 public surface. User-level atomic operations, if later required, receive a separately specified API and may not expose kernel masking semantics.

### 2.4 Public time surface

Version 1 exposes only relative `rts_task_delay(rts_tick_t delay)` and the `rts_tick_t` type. Zero-delay behavior is explicitly defined as yield-equivalent or rejected during API design; it cannot silently differ by build. No current-tick query, absolute time, conversion helper, software timer, or timer-source API is public.

Application configuration selects the tick rate as a compile-time constant. Applications own physical-unit conversion so the kernel ABI does not promise rounding semantics prematurely.

### 2.5 Architecture, MCU, board, startup, and timer boundaries

- **Cortex-M4F port:** exception frame, PSP/MSP control, PendSV, SVC startup, interrupt-mask primitives, barriers, FPU context rules, and architecture-priority constraints.
- **S32K148 target integration:** device startup linkage, clock initialization, SysTick source initialization, NVIC priority values, vector binding, and target fault/bring-up support.
- **Startup code:** supplied by the selected device support package or target integration; it initializes memory and reaches the example entry point. It is not part of the portable kernel.
- **Timer source:** S32K148 integration owns hardware setup and calls the kernel's private tick entry. The kernel owns elapsed tick semantics and delay expiry.
- **Board support:** absent. The first demonstration does not justify a separately reusable board abstraction.

The portable kernel includes neither CMSIS nor NXP SDK headers.

### 2.6 Configuration ownership

`rts_config` is a CMake INTERFACE target that contributes exactly one application-selected `rts_config.h`. That public configuration contains scheduler capacities and choices: maximum tasks, priority count, tick rate, time slicing enablement and quantum, assertion enablement, and stack alignment constraints that are part of application integration.

`kernel/config_internal.h` validates those values with preprocessor/static assertions and derives masks, widths, and internal constants. `ports/arm/cortex_m4f/port_config.h` validates architecture assumptions. `targets/nxp/s32k148/target_config.h` contains clock, interrupt priority, timer, and device integration values. Host tests select `tests/config/rts_config.h`; the target example selects `examples/s32k148_basic/rts_config.h`.

The portable kernel never includes `target_config.h`. Invalid ranges, inconsistent time-slice settings, priority-bit errors, and unsupported stack alignment fail at compile time.

### 2.7 Policy and ready-set scope

There is no runtime policy interface and no function-pointer table in version 1.

- **Ready set (`ready_queue.c`):** owns ready-list links, insert-at-tail, remove, highest-ready lookup, same-priority rotation, and same-priority-peer query.
- **Fixed-priority policy:** embodied by ready-set ordering and a small comparison rule; it is not a separate source module.
- **Scheduler core:** owns task state, invokes ready-set operations, determines whether a newly ready task outranks the running task, manages quantum expiry, and requests switching.

The private functions required initially are `ready_insert`, `ready_remove`, `ready_peek_highest`, `ready_has_peer`, and `ready_rotate`. A future EDF change replaces or generalizes this compile-time ready-set dependency under a new ADR; version 1 does not carry unused task-attribute-change hooks.

### 2.8 Task and wait-state ownership

The minimum states are `DORMANT`, `READY`, `RUNNING`, and `BLOCKED`. A blocked TCB contains a private wait record with a reason enumeration and wake result. Version 1 defines only `NONE` and `DELAY`; future reason values are documented but not compiled as synchronization infrastructure.

The TCB contains distinct intrusive nodes for the ready queue and delay queue so ownership is unambiguous. `scheduler.c` alone changes task state and wait reason. `delay_queue.c` alone links/unlinks the delay node. No generic wait-object pointer is required until an object-wait feature is designed.

### 2.9 Diagnostics ownership

Mandatory baseline correctness mechanisms are compile-time configuration validation and kernel assertions for impossible states, link ownership, stack alignment, and legal transitions. Assertion failure calls one target/application-supplied fatal hook and never attempts recovery.

Trace hooks, context-switch counters, stack watermarking, and runtime statistics are deferred. When introduced they must be compile-time removable; disabled builds contain no calls, storage, or changed scheduling decisions. Diagnostics may observe correctness but never define it.

### 2.10 Application ownership

The repository is a reusable scheduler library plus tests and examples. `examples/s32k148_basic` is the hardware demonstration and integration proof, not product application code. Board bring-up that is unrelated to scheduler validation belongs in the consuming platform project. Host examples are unnecessary for the first milestone because unit tests exercise portable behavior.

## 3. Approved layers

| Layer | Exact responsibility and owned modules | May depend on | Must never depend on | Portable | Host build | Application-visible |
|---|---|---|---|---|---|---|
| Public API | Lifecycle and task service declarations in `rts.h`, `rts_task.h`, `rts_types.h` | C11 fixed-width definitions, selected public config where required | TCB layout, port, CMSIS, SDK, target | Yes | Yes | Yes |
| Portable kernel core | Scheduler state, task transitions, idle task, tick/yield/delay orchestration in `scheduler.c` and task initialization in `task.c` | Ready and delay structures, private port contract, public types/config | CMSIS, NXP SDK, target config | Yes | Yes | No |
| Kernel data structures | Intrusive ready queues and ordered delay queue | Private TCB/node contracts and derived kernel config | Port, target, application | Yes | Yes | No |
| Port contract | Minimal operations for critical masking, switch request, initial context, and first start | Public scalar types and private TCB access contract | Policy or queues, target SDK | Conceptually | Yes | No |
| Cortex-M4F port | Exception frame, PendSV, SVC, PSP/MSP, masks, FPU rules | Port contract, CMSIS core definitions, architecture config | NXP peripheral SDK, ready/delay queues, application | Across compatible Cortex-M4F MCUs | No | No |
| Host port | Deterministic test substitute for port operations | Port contract and test support | CMSIS, NXP SDK | Host-only | Yes | No |
| S32K148 target integration | Clock/timer/NVIC/vector integration and tick delivery | Cortex port, device support, target config, private kernel tick entry | Ready/delay internals | No | No | No |
| Example | Caller-owned static stacks, selected config, target demonstration | Public API, S32K148 integration | Kernel-private headers and TCB | No | No | Executable application |
| Tests | Unit and target validation | Public API or explicit private test target as appropriate | Production-only hidden access through ad hoc includes | Test-only | Unit tests yes | No |

## 4. Minimum production module set

The first milestone has six portable implementation units: scheduler core, task initialization, ready queue, delay queue, public API boundary (which may be compiled with scheduler core if no separate translation unit is useful), and kernel assertions. The target adds the Cortex-M4F port and S32K148 timer/clock/vector integration. There are no production modules for a dispatcher, round-robin policy, idle subsystem, generic diagnostics, synchronization, EDF, deletion, MPU, SMP, or tickless operation.

## 5. Final repository tree and file responsibilities

Only files with a Sprint 0–Sprint 5 purpose are listed. The tree is a baseline, not an instruction to create empty placeholders.

```text
real_time_scheduler/
├── CMakeLists.txt
├── cmake/
│   ├── arm-none-eabi-toolchain.cmake
│   └── warnings.cmake
├── include/rts/
│   ├── rts.h
│   ├── rts_task.h
│   └── rts_types.h
├── kernel/
│   ├── scheduler.c
│   ├── scheduler_internal.h
│   ├── task.c
│   ├── task_internal.h
│   ├── ready_queue.c
│   ├── ready_queue.h
│   ├── delay_queue.c
│   ├── delay_queue.h
│   ├── intrusive_list.h
│   ├── config_internal.h
│   ├── port.h
│   └── assert_internal.h
├── ports/
│   ├── arm/cortex_m4f/
│   │   ├── port.c
│   │   ├── port_asm.S
│   │   └── port_config.h
│   └── host/
│       └── port.c
├── targets/nxp/s32k148/
│   ├── CMakeLists.txt
│   ├── target.c
│   ├── target_config.h
│   └── linker/s32k148.ld
├── examples/s32k148_basic/
│   ├── CMakeLists.txt
│   ├── main.c
│   └── rts_config.h
├── tests/
│   ├── CMakeLists.txt
│   ├── config/rts_config.h
│   ├── unit/test_scheduler.c
│   ├── unit/test_ready_queue.c
│   ├── unit/test_delay_queue.c
│   ├── unit/test_task.c
│   └── target/s32k148_smoke.c
└── docs/
    ├── architecture/sprint-0-baseline.md
    └── adr/README.md
```

File responsibilities:

- `CMakeLists.txt` defines the top-level options and composes library, port, configuration, tests, and example targets.
- `cmake/arm-none-eabi-toolchain.cmake` selects and describes the bare-metal ARM compiler toolchain without embedding scheduler policy.
- `cmake/warnings.cmake` centralizes warning and language-standard settings shared by repository targets.
- `include/rts/rts.h` declares scheduler initialization and start operations and no task representation.
- `include/rts/rts_task.h` declares static task creation, yield, relative delay, and opaque task-handle operations.
- `include/rts/rts_types.h` defines public scalar types, status values, task entry signature, opaque handle, and task creation descriptor.
- `kernel/scheduler.c` owns scheduler state, task-state transitions, current task, idle task, tick processing, quantum accounting, and switch requests.
- `kernel/scheduler_internal.h` declares the narrow kernel-wide contracts required by the port and target tick entry.
- `kernel/task.c` initializes a scheduler-reserved typed pool slot and immutable task attributes without making a task runnable independently.
- `kernel/task_internal.h` defines the private TCB, task states, wait record, and task-specific internal operations.
- `kernel/ready_queue.c` exclusively owns fixed-priority ready-set insertion, removal, highest selection, FIFO order, and rotation.
- `kernel/ready_queue.h` exposes ready-set operations only to portable kernel modules and tests.
- `kernel/delay_queue.c` exclusively owns the wake-time-ordered delayed-task collection and expiry extraction.
- `kernel/delay_queue.h` exposes delay-queue operations only to portable kernel modules and tests.
- `kernel/intrusive_list.h` supplies private constant-time intrusive-list primitives shared by the two queue owners without owning scheduling semantics.
- `kernel/config_internal.h` validates application configuration and derives portable kernel constants at compile time.
- `kernel/port.h` defines the private architecture-port contract consumed by the portable kernel.
- `kernel/assert_internal.h` defines compile-time-removable kernel assertions and the fatal assertion hook contract.
- `ports/arm/cortex_m4f/port.c` implements initial context construction, critical masking, switch request, first-task SVC launch, and C-visible exception support.
- `ports/arm/cortex_m4f/port_asm.S` implements only the register save/restore and exception-return sequences that cannot be expressed safely in C.
- `ports/arm/cortex_m4f/port_config.h` validates Cortex-M4F exception priorities, FPU policy, alignment, and architecture assumptions.
- `ports/host/port.c` implements the port contract as a deterministic host-test substitute and records switch/start requests for assertions.
- `targets/nxp/s32k148/CMakeLists.txt` integrates device support, startup objects, linker script, Cortex port, and target settings.
- `targets/nxp/s32k148/target.c` configures clocks, SysTick, NVIC/vector bindings, and forwards timer ticks into the private kernel entry.
- `targets/nxp/s32k148/target_config.h` contains S32K148 clock, timer, implemented-priority-bit, and exception-priority selections.
- `targets/nxp/s32k148/linker/s32k148.ld` defines the demonstration image memory layout and stack/section placement.
- `examples/s32k148_basic/CMakeLists.txt` composes the target demonstration executable and selects its configuration.
- `examples/s32k148_basic/main.c` supplies static task objects/stacks and demonstrates start, delay, yield, preemption, and time slicing on hardware.
- `examples/s32k148_basic/rts_config.h` selects scheduler capacities and behavior for the hardware demonstration.
- `tests/CMakeLists.txt` builds host unit executables with the host port and the target smoke image with target integration.
- `tests/config/rts_config.h` selects small deterministic capacities and enabled checks for host tests.
- `tests/unit/test_scheduler.c` verifies scheduler state transitions, preemption decisions, tick/yield behavior, and switch requests.
- `tests/unit/test_ready_queue.c` verifies priority ordering, equal-priority FIFO behavior, removal, and rotation invariants.
- `tests/unit/test_delay_queue.c` verifies wake ordering, wrap-safe comparisons, simultaneous expiry, and removal invariants.
- `tests/unit/test_task.c` verifies static object validation, initial state, stack constraints, and descriptor handling.
- `tests/target/s32k148_smoke.c` provides a minimal on-target proof of SVC startup, PSP/MSP separation, PendSV switching, delay, and tick operation.
- `docs/architecture/sprint-0-baseline.md` is the approved architectural boundary and ownership reference.
- `docs/adr/README.md` indexes Sprint 0 decisions and their status until individual ADR documents are warranted.

Vendor startup and CMSIS/NXP device files are consumed dependencies, not copied into the scheduler tree unless licensing and reproducibility requirements later mandate vendoring.

## 6. Public header policy

Version 1 has exactly three public headers:

| Header | Exposes | Why public | Must remain hidden |
|---|---|---|---|
| `rts/rts.h` | Scheduler lifecycle status and initialization/start declarations | Every application must control scheduler lifecycle | current task, tick ISR entry, locking, queues, port operations |
| `rts/rts_task.h` | Static task creation, yield, relative delay, and task-handle-level services | These are the initial application task services | TCB fields, state mutation, wait records, context frames |
| `rts/rts_types.h` | Status/type definitions, opaque `rts_task_handle_t`, task entry type, and static creation descriptor | Shared stable types avoid duplicate declarations | intrusive nodes, saved SP, raw exception state, SDK/CMSIS types |

The task descriptor accepts caller-owned stack storage only; private TCB storage comes from the kernel pool and the public API never exposes its definition. No umbrella header includes private configuration or target headers.

## 7. Private header policy

Private headers are divided into:

1. **Module contracts**, colocated in `kernel/` (`ready_queue.h`, `delay_queue.h`, `task_internal.h`), visible only to `rts_kernel` and dedicated white-box test targets.
2. **Narrow kernel-wide contracts** (`scheduler_internal.h`, `port.h`, `config_internal.h`, `assert_internal.h`), used only where cross-module coordination is unavoidable.
3. **Port/target-local configuration**, colocated with its implementation and not added to public include paths.

There is no omnibus `kernel_internal.h`. `rts_api` exports only `include/`. `rts_kernel` adds `kernel/` as PRIVATE. Port implementations receive `kernel/` privately to implement `port.h`. Queue tests link a dedicated object/static test target with `kernel/` PRIVATE; applications cannot acquire that path transitively.

## 8. Conceptual CMake target graph

```text
                         rts_config_<selection> [INTERFACE]
                                  ^
                                  |
rts_api [INTERFACE] <-------- rts_kernel [STATIC] -------> rts_port_contract [INTERFACE]
                                  |                              ^
                                  |                              |
                    +-------------+----------------+             |
                    |                              |             |
          rts_port_host [STATIC]       rts_port_cortex_m4f [STATIC]
                    ^                              ^
                    |                              |
rts_unit_* [EXECUTABLE]          rts_target_s32k148 [STATIC]
                                                   ^
                                                   |
                                  rts_example_s32k148_basic [EXECUTABLE]
                                                   |
                                  rts_target_smoke [EXECUTABLE]
```

- `rts_api` is INTERFACE and publishes `include/` as INTERFACE.
- `rts_config_<selection>` is INTERFACE and publishes the directory containing the selected `rts_config.h` as INTERFACE.
- `rts_port_contract` is INTERFACE; it carries no source and does not publish `kernel/` to applications.
- `rts_kernel` is STATIC, links `rts_api`, the selected config, and exactly one port implementation through PRIVATE composition; its `kernel/` include directory is PRIVATE.
- `rts_port_host` is STATIC, uses `kernel/` and test-support includes as PRIVATE, and has no CMSIS/NXP dependency.
- `rts_port_cortex_m4f` is STATIC, uses `kernel/`, Cortex port directory, and CMSIS core includes as PRIVATE.
- `rts_target_s32k148` is STATIC, links `rts_port_cortex_m4f` and device support privately and publishes only target link requirements needed by target executables.
- `rts_unit_scheduler`, `rts_unit_task`, `rts_unit_ready_queue`, and `rts_unit_delay_queue` are host EXECUTABLE targets linked with `rts_kernel`, `rts_port_host`, and the test configuration.
- `rts_example_s32k148_basic` is an EXECUTABLE linked with `rts_kernel`, `rts_target_s32k148`, and the example configuration.
- `rts_target_smoke` is an EXECUTABLE with the same target composition and a test-specific entry point.

CMake must reject linking both ports or multiple configuration targets into one final image. The target graph contains no CMSIS or NXP dependency on the host path.

## 9. Non-negotiable architecture invariants

1. Production scheduler code performs no dynamic allocation and calls no allocator.
2. The portable kernel performs no direct hardware-register access.
3. Public headers contain no CMSIS, NXP SDK, MCU, board, or compiler-specific types.
4. Applications never include, allocate by definition, inspect, or mutate the TCB type.
5. Scheduling-policy or ready-set code never requests or performs a context switch.
6. Only `ready_queue.c` mutates ready links; only `delay_queue.c` mutates delay links.
7. Only the scheduler core changes runtime task state and wait reason.
8. Scheduler APIs never block unless blocking is explicit in the API contract.
9. Interrupt-context work contains no unbounded traversal unless its bound and WCET rationale are documented and reviewed.
10. Diagnostic enablement cannot alter scheduler decisions or correctness.
11. Ordinary task APIs are rejected or prohibited from ISR context; future ISR APIs are explicitly named and separate.
12. No public API depends on PendSV, SVC, PSP, MSP, BASEPRI, or another Cortex-M concept.
13. The architecture port never selects the next runnable task.
14. The target timer never manipulates task or queue state directly; it invokes the private tick entry.
15. `current_task` has one owner: the scheduler core.
16. A task can belong to at most one ready queue and one wait queue, as checked by assertions in diagnostic builds.
17. All application task stacks exist before scheduler start and have application-defined static lifetime; TCBs come only from the private static pool.
18. Configuration inconsistencies fail compilation; they do not silently choose defaults.
19. PendSV is the only normal post-start context-save/restore path on Cortex-M4F.
20. Tasks execute on PSP; exceptions and startup execute on MSP.
21. Critical sections restore the prior mask state and are not exposed to applications.
22. Scheduler behavior is deterministic for a fixed event sequence and configuration.

## 10. Sprint 0 ADR index

| ADR | Title | Decision summary | Status | Main alternatives rejected |
|---|---|---|---|---|
| ADR-0001 | Static memory only | All scheduler objects and stacks have statically provisioned storage. | Proposed | Heap allocation; kernel object pool allocated at runtime |
| ADR-0002 | Single-core execution model | Kernel state and critical-section rules assume one Cortex-M core. | Proposed | SMP-ready baseline; global spinlocks |
| ADR-0003 | Fixed-priority preemptive policy | Highest ready priority runs and a higher-priority wakeup preempts. | Proposed | Cooperative baseline; runtime-selectable policies; EDF first |
| ADR-0004 | Equal-priority FIFO and optional slicing | FIFO is default; tick quantum rotates only peers at the same priority. | Proposed | Separate round-robin policy; global cyclic scheduler |
| ADR-0005 | Private static task pool | The kernel owns `RTS_MAX_TASKS` typed application TCBs plus a separate idle TCB; applications own stacks. | Amended/approved | Opaque byte overlays; exposed TCB; dynamic allocation |
| ADR-0006 | Portable kernel/port boundary | Kernel uses a minimal private port contract; hardware and exception mechanics remain outside it. | Proposed | CMSIS calls in kernel; broad generic HAL |
| ADR-0007 | Target-owned timer source | S32K148 SysTick integration emits private ticks; portable time logic does not own hardware. | Proposed | SysTick hard-coded in kernel; generic public timer framework |
| ADR-0008 | PSP/MSP separation | Tasks run on PSP and exceptions/startup use MSP. | Proposed | Tasks on MSP; configurable stack selection |
| ADR-0009 | PendSV switch mechanism | PendSV is the sole normal post-start context switch exception. | Proposed | Switching in SysTick; direct task-context switching |
| ADR-0010 | SVC first-task startup | SVC establishes the first task's exception-return context. | Proposed | Direct branch from privileged startup; special first PendSV path |
| ADR-0011 | Composed compile-time configuration | Exactly one application config plus private kernel, port, and target validation is selected per image. | Proposed | Global cross-layer config header; runtime configuration |
| ADR-0012 | Minimal public API | Three public headers expose opaque handles and no critical, clock, port, or target API. | Proposed | Public TCB; public kernel critical sections; broad time service |
| ADR-0013 | Host-replaceable port | Portable kernel tests link a deterministic host implementation of the private port contract. | Proposed | Target-only tests; mocking CMSIS calls inside kernel |

## Approved Sprint 0 Architecture Baseline

The approved repository tree is exactly the tree in section 5; empty future-feature directories and placeholder production files are forbidden.

Final ownership is: `scheduler.c` owns scheduler state, current task, runtime task transitions, idle behavior, ticks, quantum, and switch requests; `task.c` owns static task validation/initialization; `ready_queue.c` exclusively owns ready links and fixed-priority/FIFO selection; `delay_queue.c` exclusively owns delay links and wake ordering; the selected port owns critical masking and execution transfer; the S32K148 target owns clocks, SysTick, NVIC/vector integration, and tick delivery.

Final dependency direction is application/example → public API → portable kernel → private port contract → selected host or Cortex-M4F port, with S32K148 target integration present only in target images. Kernel data structures are private dependencies of the portable core. Target, SDK, and CMSIS dependencies never flow upward.

Final public headers are `rts/rts.h`, `rts/rts_task.h`, and `rts/rts_types.h`.

Final CMake targets are `rts_api`, one `rts_config_<selection>`, `rts_port_contract`, `rts_kernel`, `rts_port_host`, `rts_port_cortex_m4f`, `rts_target_s32k148`, four `rts_unit_*` executables, `rts_target_smoke`, and `rts_example_s32k148_basic`.

The final architectural invariants are the 22 review-testable rules in section 9 and are mandatory for all subsequent API and implementation work.

Deferred features are EDF, RMS admission/priority derivation, synchronization objects, task deletion, dynamic allocation, public scheduler locking, public clock/time services, trace/statistics/watermark diagnostics, tickless idle, MPU support, SMP, and a board abstraction.
