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
   StoreRowRefs, and invokes one `store-watch` callback on the uLisp task
   using the public `store-status`, `store-row-count`, `store-row-at`, and
   `store-row-field` accessors;
   and
6. delivers one later-turn `app-initialise` event, copies and redelivers two
   app-owned `poke` payloads without retaining Lisp pointers, and proves that
   binding and mounting occur only in the application's requested stages; and
7. declares a bounded UiRef/item-template/list/mount with `ui-bind`, `ui-type`,
   `ui-template`, `ui-template-list`, `ui-field`, and `ui-text`, then renders
   both imported to-dos after the StoreRef reaches ready, alongside the
   watch's ready status, bounded-row count, named `desc` field read, and
   preserved pending/nil old snapshot; and
8. projects those renderer-resolved list fields into the `#silos-app` Browser
   DOM surface without adding input handling or a browser-to-store path.

This proof exercises the corresponding subset of the proposed
[UI API](../../../docs/design/API-UI.md); it does not commit the experiment to
every deferred design point in that proposal.

The source loader preserves the current insertion order of its in-memory rows.
That is intentionally only a bootstrap simplification; editable source will
need the later linked-list head/next-row ordering model.

This is deliberately a read-only StoreRef experiment. It does not yet define
Lisp record literals or implement row creation, updates, deletion, semantic
input, or persistence. Its public Lisp source never traverses StoreRef or
StoreRowRef `meta`/`value` records directly; those compact association lists
remain runtime-private behind the typed Store accessors.

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

The importer is deliberately bounded: at most 8 stores, 64 rows per store, 4
fields per row, 15-character names, 255-character field values/source lines,
and 4096 bytes per input file. CSV accepts LF or CRLF records, quoted commas,
and doubled quotes; it rejects malformed quotes, lone CR, quoted line breaks,
NUL bytes, unsupported extensions, and all capacity overflows.

The entry registers two deliberately different callbacks. `store-watch`
is the documented storage callback: it receives `(live old-value)` only after
the live StoreRef has changed. `app-start` separately retains an app-level
handler. It receives `app-initialise` later and uses two generic `poke` events
to choose when to bind storage and when to mount the list. The Shell never
interprets that app-owned stage data. Only one StoreRef watch is supported now,
and its callback is rooted until the future app-stop/reload lifecycle releases
it. There is no watch removal operation in this increment.

Configure and test with:

```powershell
emcmake cmake -S experiments/browser-todo-prototype/runtime -B experiments/browser-todo-prototype/build -G Ninja
cmake --build experiments/browser-todo-prototype/build
ctest --test-dir experiments/browser-todo-prototype/build --output-on-failure
```

To rebuild and view the current Browser proof, run this from a Bash environment
(such as Git Bash or WSL) after its Emscripten build has been configured:

```bash
bash ./experiments/browser-todo-prototype/view-browser.sh
```

It rebuilds the configured build, serves only on `127.0.0.1:8765`, and prints
the URL to open. Pass a different local port as the first argument if needed;
use Ctrl-C to stop and clean up the server. If no configured Browser build
exists, the script prints the required `emcmake cmake` command instead of
silently configuring a non-Browser build.

The page loads adjacent Emscripten JavaScript, WASM, and preloaded data files.
When its StoreRef becomes ready, `#silos-todo-list` contains the two bound rows.
Each row's `.silos-template-field` elements retain the declared field name in
`data-field` and display the template-width-chopped value. The page is a
one-way display adapter: it does not handle input or call back into the Store.
