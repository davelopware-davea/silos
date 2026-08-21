# Browser to-do prototype journal

**Experiment plan:** [plan.md](plan.md)

## 2026-08-20 - Fresh upstream baseline

### Question

How can this prototype retain a simple, trustworthy record of every direct
change made to FreeRTOS and uLisp after the experiment begins?

### Method

- Created the `codex/browser-todo-prototype` branch and this separate
  experiment tree.
- Retrieved fresh upstream checkouts of FreeRTOS-Kernel V11.3.0 and uLisp ESP
  4.9a, then verified their checked-out commits as
  `9b777ae5c5b8e9e456065a00294d1e5f5f9facf5` and
  `aa9b24ca3323159dacadca60ea0e9ffdf00b1a81`, respectively.
- Removed the nested Git metadata so both complete source snapshots are normal
  files tracked by the SilOS repository.
- Kept all provenance and experiment documentation outside the upstream source
  directories.

### Result

The full, unmodified source snapshots are ready to be reviewed and committed
as the experiment baseline. No FreeWisp runtime code or source adaptations
have been copied into this tree yet.

### Next question

After the baseline commit, what is the smallest FreeWisp runtime slice that
can support loading and exercising the first real uLisp to-do application?

## 2026-08-20 - In-memory to-do source boot and load

### Question

Can the Browser runtime seed an in-memory source backend at boot, discover a
minimal manifest, and load a real uLisp to-do application from the stores it
just seeded?

### Method

- Reused the proven FreeWisp cooperative Browser port while compiling against
  this experiment's clean FreeRTOS and uLisp snapshots.
- Added a fixed-capacity SRAM-style source backend. Boot inserts source rows
  for `apps/todo/app.lisp` and `apps/todo/src/main`, then reads them back
  through store lookup; it never evaluates the compiled-in source strings
  directly.
- Temporarily discovered exact `apps/<app-name>/app.lisp` names under `apps/`.
  The manifest is trusted to contain its one `app-declare` form. Its declared
  entry store is then streamed row-by-row into the uLisp reader.
- Added generated, experiment-owned uLisp adapters for `app-declare` and
  `app-start`; they leave the vendored uLisp source unchanged. `app-start`
  retains its closure as a garbage-collector root.
- Wrote the to-do entry as a lexical event-handler closure, with block comments
  explaining the Lisp. The pinned reader's semicolon-comment path caused a
  malformed-list error for streamed rows, while its documented `#| ... |#`
  block-comment path worked.

### Observed result

- CTest built the Emscripten/Asyncify target and passed the complete boot
  proof. The to-do handler reported its two seeded rows, added an in-memory
  sample row, reported three rows, and returned the first row's `"to do"`
  status.
- A direct Node 24.14 run immediately after the test was denied access to the
  OneDrive-generated JavaScript file (`EPERM`). CTest had already executed the
  same module successfully using the Emsdk Node 24.19 runtime, so this is a
  host file-lock artifact rather than an application failure.
- `git diff` reported no changes below either this experiment's
  `third-party/FreeRTOS-Kernel` or `third-party/ulisp-esp` directories.

### Next question

What smallest subset of the documented StoreRef and StoreRowRef API is needed
to replace the to-do closure's private list with volatile, in-memory rows?

## 2026-08-20 - Volatile read-only StoreRef binding

### Question

Can the loaded to-do app receive seeded in-memory rows through the documented
StoreRef/StoreRowRef shape, including an asynchronous pending-to-ready bind,
rather than retaining a private Lisp list?

### Method

- Extended the fixed-capacity in-memory backend with a `todo/items` rows store
  containing two seeded records (`desc` and `status`).
- Added native `store-bind` and `field` primitives outside the vendored uLisp
  tree. The first bind returns a rooted StoreRef with `bind`, `pending`, and
  `nil` value metadata, then sends a bounded request to a storage task.
- The storage task returns a bounded completion. Only the uLisp task converts
  the native rows into live StoreRowRefs, sets the collection value, and moves
  the StoreRef status to `ready`.
- Changed the application source to capture the returned StoreRef lexically,
  read its metadata/row value using `field`, and retain explanatory uLisp block
  comments. It performs no row writes.

### Observed result

- CTest passed the pending-to-ready, bound-count (`2`), and row `desc` field
  read proof in 0.51 seconds.
- The cooperative Browser port uses a 100 Hz tick. `pdMS_TO_TICKS(1)` rounded
  to zero and caused the uLisp polling loop to busy-spin, preventing the
  storage task from running. Waiting one explicit tick restored the bounded
  queue handoff.
- The pinned uLisp packs short reader symbols such as `desc` and `status` as
  radix-40 values, while native-generated field keys are long symbols. The
  native field lookup must compare according to the stored representation.
- No files under either vendored FreeRTOS or uLisp source tree changed.

### Next question

What record-literal representation should `store-row-add` accept before the
next phase adds app-side row creation?

## 2026-08-20 - Generic volatile store catalogue

### Question

Can the boot-loaded Lisp source and volatile to-do rows use the same bounded
in-memory store representation, without the backend encoding either row shape?

### Method

- Split the in-memory backend from the Browser runtime into
  `InMemoryStoreBackend.h` and `.cpp`, and moved all hard-coded boot data into
  `BootSeed.cpp`.
- Replaced the source-store and to-do-store union with one catalogue of exact
  store names. Each row carries stable `id`/`revision` metadata and a bounded
  list of named string fields.
- Seeded source as ordinary rows with a `text` field and to-dos as ordinary
  rows with `desc` and `status` fields. The source reader and StoreRef adapter
  both retrieve fields by name through the same backend lookup.
- Kept `store-bind` and `field` as the documented Lisp-facing operations. The
  adapter now copies bounded field names from Lisp symbols and honours its
  `start`/`count` window while building StoreRowRefs.

### Observed result

- Emscripten configure/build succeeded. The first CTest attempt hit the known
  transient OneDrive `EPERM` lock while Node opened the generated bundle; the
  unchanged retry passed `silos_todo_boot` in 0.52 seconds.
- The passing test retained the pending-to-ready bind, count (`2`), and named
  `desc` field-read proof. No files under either vendored upstream tree changed.

### Next question

What schema-ordered Lisp record literal should `store-row-add` accept before
the next phase adds row creation?

## 2026-08-21 - Semicolon comments in streamed source rows

### Question

Can the versioned Lisp sources use normal semicolon comments while being
streamed from generic `text` rows, rather than relying on block comments?

### Method

- Replaced the versioned app manifest and entry-source `#| ... |#` comments
  with equally detailed `;` line comments.
- Made the source-reader contract explicit: after consuming each imported
  `text` row it emits one newline character before starting the next row. The
  importer stores line text without its original delimiter, so this boundary
  is necessary for the uLisp reader to terminate a semicolon comment.

### Observed result

- CTest loaded and evaluated the semicolon-commented manifest and entry store,
  then completed the StoreRef watch proof successfully. This identifies the
  earlier malformed-list observation as a streamed-row delimiter issue, not a
  requirement to use block-comment syntax.
- No FreeRTOS or uLisp vendor file changed.

### Next question

What schema-ordered Lisp record literal should `store-row-add` accept before
the next phase adds row creation?

## 2026-08-21 - Read-only StoreRef watch

### Question

Can the loaded app observe its bound StoreRef through the documented
`store-ref-watch` callback, rather than treating its app-level handler as a
storage notification mechanism?

### Method

- Added `store-ref-watch` with the documented `(store-ref handler)` shape. Its
  callback is a native GC root alongside the live StoreRef.
- When the uLisp task receives the bound-store completion, it first builds a
  fresh bounded snapshot of the pending StoreRef, then replaces the live value
  with StoreRowRefs and changes its status to ready. It invokes the stored
  callback exactly once after that mutation, on the same uLisp task.
- Changed the versioned Lisp entry to attach a heavily commented watch lambda.
  The lambda reads `live` status, rows, and sample `desc`, then compares the
  old snapshot status/value through a clearly test-only native observation
  sink. The `app-start` closure now only returns its future Shell event.
- Removed the old test event/evaluation queue so the proof cannot invoke the
  app handler to inspect StoreRef state.

### Observed result

- Verbose CTest reported `store-bind=todo/items.csv status=ready watch=1` and
  `store-ref-watch fired=1 ready=yes count=yes old=pending/nil`, followed by
  `SILOS_TODO_BOOT_PASS` in 0.45 seconds.
- The callback observes the live ready value after mutation while the old
  snapshot remains pending with nil value. The app handler is registered but
  never invoked by this storage proof.
- One StoreRef watch is currently supported and remains rooted for the active
  app lifetime. Watch removal and app-stop/reload release are intentionally
  deferred. No vendored FreeRTOS or uLisp file changed.

### Next question

What schema-ordered Lisp record literal should `store-row-add` accept before
the next phase adds row creation?

## 2026-08-21 - Versioned startup-store import

### Question

Can Browser startup populate the volatile generic store catalogue from
versioned input files, rather than from compiled C++ source and to-do constants?

### Method

- Added `runtime/store-init/` as the single startup input tree and configured
  Emscripten to preload it into `/store-init` in the generated Wasm artifact.
  CMake tracks every input file as a link dependency so changed inputs rebuild
  the preload data.
- Added a bounded recursive importer. The logical store name is each exact,
  normalized relative filename: `apps/todo/app.lisp`,
  `apps/todo/src/main.lisp`, and `todo/items.csv`.
- Imported each `.lisp` file as insertion-ordered generic `text` rows. Imported
  CSV headers as generic field names and CSV data records as generic rows with
  loader-assigned sequential IDs and revision `1`.
- Removed `BootSeed.cpp`/`.h`; neither app source nor to-do data remains as
  compiled C++ constants. Updated the manifest entry and app bind to the new
  extension-bearing logical store names.

### Observed result

- The Browser target configured, rebuilt, and passed CTest with the files
  preloaded from `/store-init`. The app was therefore discovered from
  `apps/todo/app.lisp`, evaluated from `apps/todo/src/main.lisp`, and bound
  the imported `todo/items.csv` rows.
- The importer rejects malformed or over-capacity input deterministically. Its
  current bounds are 4096 bytes per file, 8 stores, 40 rows/store, 4 fields/row,
  15-character field names, and 255-character source lines or field values.
  CSV supports quoted commas and doubled quotes, but deliberately rejects
  quoted line breaks and lone CR line endings.
- No vendored FreeRTOS or uLisp file changed.

### Next question

What schema-ordered Lisp record literal should `store-row-add` accept before
the next phase adds row creation?
