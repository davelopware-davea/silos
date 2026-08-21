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
