# Browser to-do bootstrap proof

This target starts the existing FreeWisp cooperative Browser port with the new
experiment's clean FreeRTOS and uLisp vendor trees. It then:

1. preloads and imports the versioned `store-init/` tree into one
   fixed-capacity in-memory store catalogue;
2. scans `apps/` for `apps/<app-name>/app.lisp` manifests;
3. evaluates each manifest directly, capturing its `app-declare` result;
4. streams and evaluates the declared entry store; and
5. proves that `store-bind` returns pending first, then the uLisp task receives
   a bounded completion, snapshots the old pending ref, exposes ready
   StoreRowRefs, and invokes one `store-ref-watch` callback on the uLisp task;
   and
6. tests the watch's ready status, bound-row count, named `desc` field read,
   and preserved pending/nil old snapshot.

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
the backend knowing either shape.

`StoreInitLoader.cpp` traverses the Emscripten-preloaded `/store-init` virtual
directory before FreeRTOS starts. A store name is the exact normalized relative
file path, including the extension:

- `store-init/apps/todo/app.lisp` -> `apps/todo/app.lisp`
- `store-init/apps/todo/src/main.lisp` -> `apps/todo/src/main.lisp`
- `store-init/todo/items.csv` -> `todo/items.csv`

The manifest consequently declares `apps/todo/src/main.lisp`, and the app
binds `todo/items.csv`. Each `.lisp` line becomes an ordered generic row with a
`text` field; the streaming reader re-emits a newline between rows, so normal
`;` Lisp comments terminate at their original source-line boundary. A `.csv`
header becomes named application fields; every data row
receives sequential loader-assigned IDs and revision `1`.

The importer is deliberately bounded: at most 8 stores, 40 rows per store, 4
fields per row, 15-character names, 255-character field values/source lines,
and 4096 bytes per input file. CSV accepts LF or CRLF records, quoted commas,
and doubled quotes; it rejects malformed quotes, lone CR, quoted line breaks,
NUL bytes, unsupported extensions, and all capacity overflows.

The entry registers two deliberately different callbacks. `store-ref-watch`
is the documented storage callback: it receives `(live old-value)` only after
the live StoreRef has changed. `app-start` separately retains an app-level
handler for later Shell lifecycle/inter-app events; this proof does not use it
to observe storage. Only one StoreRef watch is supported now, and its callback
is rooted until the future app-stop/reload lifecycle releases it. There is no
watch removal operation in this increment.

Configure and test with:

```powershell
emcmake cmake -S experiments/browser-todo-prototype/runtime -B experiments/browser-todo-prototype/build -G Ninja
cmake --build experiments/browser-todo-prototype/build
ctest --test-dir experiments/browser-todo-prototype/build --output-on-failure
```
