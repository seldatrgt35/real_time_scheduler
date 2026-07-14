# Sprint 3.1 — Intrusive List Implementation

## Scope

Sprint 3.1 implements only the generic, private doubly linked-list mechanism declared by `kernel/intrusive_list.h`. It contains no task, priority, delay, scheduler, timer, port, or context-switch semantics.

## Implemented operations

| Operation | Contract | Complexity |
|---|---|---:|
| `rts_list_initialize` | Establish canonical empty head, tail, and count | O(1) |
| `rts_list_node_initialize` | Establish canonical unlinked node | O(1) |
| `rts_list_is_empty` | Query `count == 0` and assert endpoint consistency | O(1) |
| `rts_list_node_is_linked` | Query the unconditional owner pointer | O(1) |
| `rts_list_push_back` | Insert an unlinked node at the tail | O(1) |
| `rts_list_insert_before` | Insert an unlinked node before a known member | O(1) |
| `rts_list_remove` | Unlink a known member and canonicalize it | O(1) |

No additional accessor, insert-after, rotation, traversal, or container-conversion API was added.

## Representation and ownership

An empty list is `{head=NULL, tail=NULL, count=0}`. Linkage is unlinked when `{previous=NULL, next=NULL, owner=NULL}`. A linked node has exactly one nonnull owner, including the singleton case. Removal clears all three linkage fields.

Each node also carries an opaque `object` backlink. Node initialization clears it, but ordinary list insertion/removal does not interpret or mutate it. The embedding ready or delay module owns association and clearing. This preserves list policy isolation while permitting strict C11 object recovery without container arithmetic.

List and node initialization write a fresh canonical representation. Callers must not reinitialize a node that is currently linked; inspecting a genuinely uninitialized C object to detect that misuse would itself be invalid, so ownership is enforced at insertion and removal boundaries.

## Assertion behavior and integer safety

The implementation uses the approved `RTS_ASSERT` mechanism for null inputs, malformed constant-time endpoint state, insertion of linked/noncanonical nodes, wrong-list positions and removals, unlinked removal, neighbor inconsistency, `SIZE_MAX` increment overflow, and zero-count decrement underflow.

Assertions add only constant-time checks. No production mutation performs validation traversal. With assertions disabled, every valid call follows the same deterministic mutation path and does not depend on assertion side effects. Physical kernel capacity is bounded by configured static objects, far below `SIZE_MAX`; the generic layer nevertheless checks the scalar boundary without depending on scheduler configuration.

## Tests

The host test covers empty/node initialization, singleton and multiple tail insertion, FIFO link order, insertion before middle and head, removal of middle/head/tail/singleton, canonical unlink restoration, opaque object preservation, constant-time ownership queries, count and endpoint correctness, independent-list isolation, double-insert assertion, wrong-owner removal assertion, and unlinked removal assertion.

A test-only forward/backward validator checks count agreement, reciprocal links, common owner, endpoint form, and finite traversal bounded by the expected count. It is not linked into production code.

## Build and verification

The Sprint uses a minimal CMake/CTest target with C11 extensions disabled and warnings promoted to errors for GNU/Clang (`-Wall -Wextra -Werror`) or MSVC (`/W4 /WX`). The intended commands are:

```text
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The production translation unit is also compiled without linking for `arm-none-eabi`, Cortex-M4, using Clang when available.

In the Codex verification environment, CMake was not installed and the Windows Clang installation had no host C runtime. The same standard C test translation unit was therefore linked as freestanding WebAssembly and executed with the bundled Node runtime. Both assertion-enabled and assertion-disabled configurations returned zero failures. The production module was separately compiled as a freestanding Cortex-M4 translation unit with `-Wall -Wextra -Werror`. The committed CMake/CTest integration remains the normal developer workflow when CMake and a host runtime are available.

## Known non-blocking limitations

- Initialization cannot safely distinguish a fresh indeterminate C object from a previously linked object; the caller must honor the initialization precondition.
- Full-list corruption detection is intentionally test-only. Production assertions validate only constant-time local and endpoint invariants.
- The list count is `size_t`; practical capacity is constrained by statically configured kernel objects, while the generic implementation guards only the absolute `SIZE_MAX` boundary.
