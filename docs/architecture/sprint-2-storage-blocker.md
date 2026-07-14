# Sprint 2 Architecture Blocker — Opaque Caller-Owned TCB Storage in C11

**Status:** Resolved by the approved kernel-owned static TCB-pool amendment.  
**Superseded by:** [Version 1 Public API Design](version-1-public-api.md#architecture-amendment-kernel-owned-static-tcb-pool) and [Sprint 2 Internal Kernel Design](sprint-2-internal-kernel.md).

## Finding

The approved public declaration makes each task object an instance of `rts_task_storage_t`, whose declared C type is a union containing `max_align_t` and an array of unsigned characters. The proposed private object would instead have declared type `struct rts_task` / `rts_tcb_t`.

The following checks are necessary but not sufficient:

```c
_Static_assert(sizeof(rts_tcb_t) <= sizeof(rts_task_storage_t),
               "private TCB exceeds the public ABI reserve");

_Static_assert(_Alignof(rts_task_storage_t) >= _Alignof(rts_tcb_t),
               "public task storage is under-aligned");
```

They prove only that an address range is large and aligned enough. They do not change the effective or declared type of the public union object. Converting `rts_task_storage_t *` to `struct rts_task *` and accessing TCB members through the result is not a strictly conforming C11 object-access model. The public union does not contain `struct rts_task` as a member, and it cannot contain an incomplete private struct as a member.

Consequently, the desired combination cannot all be guaranteed by portable C11:

1. the application instantiates a complete public storage type;
2. the actual private TCB is a different, hidden structure type;
3. the TCB lives directly inside that declared public object;
4. kernel code accesses it as a normal typed structure;
5. strict-aliasing rules are respected;
6. no compiler-specific aliasing contract is used.

`max_align_t`, a larger byte reserve, raw `void *` storage, or a declaration macro does not resolve the declared-type problem. Host-versus-ARM alignment differences and TCB growth are secondary risks; the language-model issue exists even when size and alignment match exactly.

## Why private design is paused

The requested Sprint 2 artifacts depend on this decision:

- the exact private TCB definition and field offsets;
- whether the public handle and private TCB are the same C type;
- strict-alias-safe handle conversion;
- ownership of the task registry or pool;
- task-creation transaction semantics;
- compile-time storage assertions;
- host-port equivalence;
- PendSV access to the saved-stack-pointer field.

Choosing a TCB and port exchange model before resolving its object-lifetime model would bake undefined behavior or a contradictory ownership rule into the approved baseline.

## Corrective choices

### Choice 1 — Kernel-owned static TCB pool (recommended)

The kernel defines a fixed array of the private complete type:

```c
static struct rts_task task_pool[RTS_MAX_TASKS];
```

The public descriptor no longer contains `task_storage`, and the public storage union and declaration macro are removed. Caller-owned byte-counted stacks remain unchanged. A public handle is a pointer to an actual `struct rts_task`, so no cast, overlay, effective-type exception, or aliasing extension is required.

Properties:

- fully static and deterministic;
- strictly conforming typed object access;
- opaque TCB and stable incomplete-pointer handles;
- capacity exhaustion remains explicit;
- memory usage is `RTS_MAX_TASKS * sizeof(rts_tcb_t)` regardless of tasks created;
- changes the approved ownership decision from caller-owned TCB storage to kernel-owned compile-time storage;
- preserves all five approved function signatures but changes `rts_task_config_t` and removes the storage public type/macro.

### Choice 2 — Expose the complete task object layout publicly

The public `struct rts_task` becomes the real object type instantiated by the application. This is strictly conforming and caller-owned, but it exposes the TCB layout, creates configuration-dependent ABI risk, and directly violates the approved opaque-TCB requirement. Not recommended.

### Choice 3 — Adopt a documented compiler aliasing extension

Retain the public storage union and define a supported-toolchain rule such as disabling strict-alias optimizations or using a compiler-specific alias annotation for private access. This matches a common embedded technique but is not a portable C11 guarantee, complicates host equivalence, and makes correctness dependent on every build target carrying identical compiler flags. Not recommended for the stated requirements.

### Choice 4 — Bytewise encoded TCB

Treat the union as the actual object and read/write every logical field through byte copies and fixed offsets. This can avoid incompatible typed lvalue access but sacrifices normal typed structure access, makes assembly offsets and validation fragile, and turns the TCB into a manual serialization format. Not recommended.

## Recommended architecture amendment

Adopt Choice 1 and amend the baseline as follows:

- retain static allocation only;
- retain caller-owned task stacks;
- replace caller-owned task-object storage with a kernel-owned, compile-time-sized private TCB pool;
- retain opaque incomplete-pointer handles, which now point directly to pool elements;
- retain `RTS_MAX_TASKS` as the exact application-task pool capacity;
- remove `RTS_TASK_STORAGE_SIZE`, `rts_task_storage_t`, `RTS_TASK_STORAGE_DECLARE`, and `rts_task_config_t.task_storage` from the public API;
- require no size/alignment bridge between public and private task types;
- keep idle-task storage separate from the application-task pool so `RTS_MAX_TASKS` continues to mean application-task capacity.

This amendment is the smallest design that satisfies static allocation, opaque handles, normal typed TCB access, strict aliasing, host portability, and predictable capacity simultaneously.

## Resolution

Corrective Choice 1 was approved. The public opaque byte-storage type was removed, `RTS_MAX_TASKS` now sizes a private typed application-task pool, and the idle task has separate private storage. Caller-owned stack storage and incomplete-pointer public handles remain unchanged. No aliasing extension or reinterpretation cast is permitted.
