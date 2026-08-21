# Browser to-do bootstrap proof

This target starts the existing FreeWisp cooperative Browser port with the new
experiment's clean FreeRTOS and uLisp vendor trees. It then:

1. seeds one fixed-capacity in-memory store catalogue with the to-do
   declaration, entry source, and two volatile `todo/items` rows;
2. scans `apps/` for `apps/<app-name>/app.lisp` manifests;
3. evaluates each manifest directly, capturing its `app-declare` result;
4. streams and evaluates the declared entry store; and
5. proves that `store-bind` returns pending first, then the uLisp task receives
   a bounded completion and exposes ready StoreRowRefs to the to-do app; and
6. tests the bound row count and a named `desc` field read through the app.

The source loader preserves the current insertion order of its in-memory rows.
That is intentionally only a bootstrap simplification; editable source will
need the later linked-list head/next-row ordering model.

This is deliberately a read-only StoreRef experiment. It does not yet define
Lisp record literals or implement row creation, updates, deletion, UI, or
persistence.

`InMemoryStoreBackend.{h,cpp}` contains the experiment-owned, fixed-capacity
catalogue. Every store has the same generic row representation: stable `id`
and `revision` metadata plus a bounded array of named string fields. Therefore
source rows use a `text` field while to-do rows use `desc` and `status`, without
the backend knowing either shape. `BootSeed.cpp` contains the hard-coded source
and to-do constants and installs them through that generic backend; `main.cpp`
only bootstraps and consumes the catalogue.

Configure and test with:

```powershell
emcmake cmake -S experiments/browser-todo-prototype/runtime -B experiments/browser-todo-prototype/build -G Ninja
cmake --build experiments/browser-todo-prototype/build
ctest --test-dir experiments/browser-todo-prototype/build --output-on-failure
```
