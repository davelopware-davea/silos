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
  current bounds are 4096 bytes per file, 8 stores, 64 rows/store, 4 fields/row,
  15-character field names, and 255-character source lines or field values.
  CSV supports quoted commas and doubled quotes, but deliberately rejects
  quoted line breaks and lone CR line endings.
- No vendored FreeRTOS or uLisp file changed.

### Next question

What schema-ordered Lisp record literal should `store-row-add` accept before
the next phase adds row creation?

## 2026-08-21 - Lifecycle poke and bounded StoreRef list render

### Question

Can the app receive a later lifecycle event, use application-owned staged poke
payloads to control binding and mounting, and render imported StoreRef rows
through bounded UI declarations without adding native stage branches?

### Method

- Added one queued `app-initialise` delivery after successful `app-start` and a
  one-outstanding-poke queue boundary. Payloads use a fixed 16-node native
  arena with index links, then become fresh `(poke . payload)` Lisp lists on
  the next uLisp event turn.
- Reconstructed short symbols with uLisp's packed radix-40 rule and longer
  symbols with its long-symbol interning rule. This preserves app-level symbol
  equality after the native queue boundary.
- Declared the documented keyword-form `defuilist` as an unbounded special
  form, preserving the public source form despite uLisp's seven-argument
  fixed-function encoding. The app uses its own `init` and numeric stages to
  bind, then declare and mount the StoreRef list.
- Raised fixed source rows/store from 40 to 64 for the now-commented 63-row
  entry source, and Browser task slots from four to five for uLisp, storage,
  Shell forwarding, client verification, and idle.

### Observed result

- Emscripten configure/build and CTest passed. The proof delivered one
  `app-initialise`, two FIFO pokes, one pending-to-ready StoreRef watch, and a
  mounted list that rendered both imported to-dos.
- The Shell did not interpret `init` or either stage number; the bind and mount
  were selected only by the app handler. No vendored FreeRTOS or uLisp source
  changed.

### Next question

What bounded semantic input event should drive the first editable to-do action?

## 2026-08-22 - Lifecycle/UI proof handoff and checkout reconciliation

### Question

Is the Browser prototype's current proof recorded in the authoritative checkout
and ready for the next editable-store design decision without carrying forward
stale-checkout or vendor ambiguity?

### Method

- Reconciled the accidental continuation in a stale OneDrive checkout into the
  authoritative `C:\Users\dave\src\SilOS` checkout using Git-normalised
  content hashes. The `src` branch is reconciled through
  `d992ad6 design: add bounded UI API proposal`, plus the current uncommitted
  lifecycle/UI/poke/cross-reference work on `codex/browser-todo-prototype`.
- Checked the proof boundary: general queue-backed `(app-request-poke arg...)`
  produces a fresh `(poke . payload)` on a later uLisp turn; the Shell also
  sends a later-turn `app-initialise`; application-owned `init` stages select
  binding and UI mounting; and the StoreRef watch drives bounded
  UiRef/template/list/mount rendering.
- Checked the versioned source and test result. Source UI forms match the
  general [UI API](../../docs/design/API-UI.md); the imported Lisp source has
  63 rows within its fixed 64-row limit; CTest passed in the authoritative
  checkout in 0.26 seconds.

### Observed result

- The two imported CSV rows render through the mounted list after the
  pending-to-ready StoreRef transition. This proof still excludes semantic
  create, edit, delete, and persistence behaviour.
- No vendor files changed. The API-Shell lifecycle/poke amendment remains
  uncommitted, as do the associated lifecycle/UI/cross-reference changes.
- Recovery material remains intentionally untouched: `stash@{0}` is named
  `codex-migration-pre-sync-2026-08-22`, and `.vscode/` is unrelated untracked
  content.

### Next question

What schema-ordered Lisp record representation and validation should
`store-row-add` require before separately delegated create/edit/delete phases,
then persistence and restart behaviour?

## 2026-08-22 - Browser-visible template-rendering gate

### Question

Does the completed bounded UI proof display its bound template on an actual
Browser surface, and should editable-store work wait until that loop is
visible?

### Method

- Inspected the checkpointed Browser target after `e146103 feat: prove browser
  todo lifecycle and UI binding`, including its template renderer and build
  target.
- Distinguished the existing native template-render diagnostic from
  Browser-surface output before choosing the next experiment phase.

### Observed result

- `silos_render_ui` streams the mounted list and template field values through
  `std::printf`; it verifies the bounded UiRef/template/list path but creates
  no DOM, canvas, or other visible Browser surface.
- The lifecycle/UI checkpoint is cleanly committed. Its boot CTest passed in
  0.80 seconds; `.vscode/` and the migration stash remain untouched.

### Next question

What is the smallest Browser surface adapter that can visibly render the
existing two bound template rows while preserving the current bounded UI model
and avoiding semantic input or store mutation?

## 2026-08-22 - Browser-visible bound-template rendering

### Question

Can the completed bounded UI proof project its template-driven list to a real
Browser surface without creating a second UI data model or introducing semantic
input/store mutation?

### Method

- Added a small `browser-surface.html` launcher beside the Emscripten output;
  it supplies the `#silos-app` document region and loads the existing runtime
  JavaScript, WASM, and preloaded store data from the same local web origin.
- Kept `silos_render_ui` as the sole renderer. Its existing resolved list
  state and bounded/chopped template fields now call a one-way Browser adapter
  which clears/rebuilds the DOM, creates a ready-state list, and appends each
  resolved field as a named element. The adapter cannot access a StoreRef and
  has no input or mutation operation.
- Rebuilt and ran CTest, then loaded the staged page through a local HTTP
  server in the Browser. Inspected both the rendered page and its DOM field
  metadata after startup completed.

### Observed result

- CTest passed in the authoritative checkout: 1/1 in 0.38 seconds.
- The real Browser surface reached `data-state="ready"` and displayed exactly
  two `#silos-todo-list` rows. The DOM showed template fields in order:
  row 0 `desc="Learn how SilOS loads Lisp from "`, `status="to do"`; row 1
  `desc="Build the first live screen bind"`, `status="in progress"`. The
  first description is correctly chopped by its declared 32-character width.
- Visual inspection showed the same two item rows and their status values on
  the page. No vendor files changed, and no semantic input or store mutation
  path was added.

### Next question

What Lisp record representation and validation should `store-row-add` require
before separately delegated create/edit/delete phases, each verified through
this browser-visible bound-template surface?

## 2026-08-22 - Repeatable Browser visual-check launcher

### Question

Can each experiment phase provide the same safe, manual path for inspecting
the current Browser proof before subsequent work begins?

### Method

- Added `view-browser.sh`, a Bash entry point that rebuilds the existing
  configured Emscripten build, requires the staged `browser-surface.html`, and
  serves the build directory with Python 3 on `127.0.0.1` only.
- Made absent build configuration and missing Python explicit failures with
  setup guidance. The server remains foregrounded until Ctrl-C and is cleaned
  up by a shell trap.
- Recorded a per-phase visual-check pause and script-maintenance rule in the
  experiment plan.

### Observed result

- The launcher introduces no runtime/UI behavior; it only exposes the existing
  staged Browser surface through a repeatable local URL.
- `bash -n view-browser.sh` passed; the launcher rebuilt the configured target
  and returned HTTP 200 for `browser-surface.html`. CTest passed 1/1 in 0.32
  seconds, and a live Browser DOM/visual check showed the existing two bound
  template rows with their expected field values.

### Next question

What record representation and validation should `store-row-add` use before
the first editable-store phase, which must again end at this visual-check gate?
