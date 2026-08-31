# RefactorPlan: UI-oriented Store classes

**Status:** implemented baseline on 30 August 2026.

This supporting plan records the Store refactor agreed after the zero-copy UI
pipeline work. Project-wide decisions remain authoritative in
[SilOS_PLAN.md](SilOS_PLAN.md).

## Intent

- Keep uLisp built-ins as thin adapters into compiled Store classes.
- Allow one app or several apps to bind the same named store independently.
- Route every request for a named store through one shared `BoundStore`.
- Materialise platform rows once as canonical rooted StoreRefs and StoreRowRefs;
  UiRefs and renderers borrow those values without another projection copy.
- Run platform storage work through the dedicated Store task without blocking
  the independent UI task.

## Class responsibilities

- `StoreEngine` owns uLisp Store policy, per-app registries, StoreRef creation,
  watches, completion projection, ownership checks, and GC integration.
- `BoundStore` owns the shared identity, platform-store association, rooted
  uLisp state, keyed-operation coordination, and mutation gate for one name.
- `StoreAppBinding` owns one app's dynamically sized binding collection.
- `StoreBinding` owns one bind call's StoreRef, watch, selected fields, row
  window, and BoundStore association.
- `StoreService` owns Store-task request dispatch and pointer-free completions.
  Bind is implemented first; mutation handlers explicitly report unsupported.
- `IPlatformStorageEngine` is the platform catalogue interface and
  `IPlatformStore` is the borrowed per-store traversal interface.
- `InMemoryStorageEngine` and `InMemoryStore` are the initial adapters.

Every class declaration documents its responsibility, owned and borrowed data,
lifetime rules, task expectations, and interactions with adjacent classes.

## Binding and writability rules

- Bindings to the same store have independent fields, windows, request state,
  StoreRefs, and watches, but share BoundStore writability.
- Request and completion messages identify the app generation, StoreBinding,
  BoundStore, and operation. Both FreeRTOS queues admit a bounded burst of eight.
- Queue rejection rolls back a new binding rather than leaving an unreachable
  pending StoreRef.
- `store-blocked-p` exposes the shared mutation gate through any associated
  StoreRef. Reads remain allowed while a mutation is pending.
- `store-wait-until-writable` accepts a required 0–1000 ms timeout, returning
  `t` when writable and `nil` on timeout. Waiting pauses the single uLisp task,
  releases its workspace mutex, and leaves Store and UI tasks runnable.
- Future Store-backed writes use `(setf (store-row-field row field) value)`.
  The setter rechecks writability, updates the existing row optimistically, and
  submits persistence. A blocked setter leaves the value unchanged and errors.

## Verification

- Exercise platform interfaces through the in-memory adapters.
- Cover repeated and cross-app binds, independent projections and watches,
  shared writability, keyed and stale completions, queue exhaustion, cleanup,
  GC relocation, and zero-copy UI traversal.
- Cover wait polling, success, timeout, races, continued UI rendering, and
  deferred watch delivery as mutation support is enabled.
- Keep Alive stubs synchronized with public API documents and finish through
  `src/test.sh`.
