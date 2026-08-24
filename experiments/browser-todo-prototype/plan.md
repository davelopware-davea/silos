# Browser to-do prototype plan

**Status:** In progress; the Browser boot/load, read-only StoreRef, bounded
lifecycle/poke/UI-model, and browser-visible template-rendering proofs are
complete, and their runtime implementation now follows the agreed SilOS code
layout. The `store-row-add` representation and validation decision is next.
Each phase pauses after its Browser visual check so the user can run
`view-browser.sh` before the next phase begins. `build.sh` and `test.sh` are the
stable from-any-directory Bash entry points for building and testing.

**Project context:** [SilOS plan](../../docs/design/SilOS_PLAN.md).

**Relevant API sketch:** [proposed UI API](../../docs/design/API-UI.md).

**Agreed code layout:**
[SilOS code layout](../../docs/design/Discussion-Code-Layout.md).

## Current status and handoff

- Work only in the authoritative `C:\Users\dave\src\SilOS` checkout, on
  `codex/browser-todo-prototype`. A prior session accidentally continued in a
  stale OneDrive checkout; its content was reconciled into `src` by
  Git-normalised hashes and verified against committed history through
  `e146103 feat: prove browser todo lifecycle and UI binding`.
- The proof has a general queue-backed `(app-request-poke arg...)` boundary
  that delivers fresh `(poke . payload)` values, a later-turn
  `app-initialise`, application-owned `init` stages, one StoreRef watch, and
  bounded UiRef/template/list/mount rendering of the two imported CSV rows.
- Source UI forms now match the general [UI API](../../docs/design/API-UI.md).
  The lifecycle/UI/cross-reference proof is checkpointed as `e146103 feat:
  prove browser todo lifecycle and UI binding`. The imported Lisp source has
  63 rows, within its fixed 64-row limit. At that checkpoint both vendor trees
  were unchanged, and CTest passed in `C:\Users\dave\src\SilOS` in 0.80
  seconds.
- The current API-consistency increment replaces the prototype-only `defui*`
  names and generic `field` traversal with the approved `ui-*`, `store-*`,
  and `store-row-*` vocabulary. Its 62-row source remains within the fixed
  64-row limit and its test proves the ready status, bounded row count,
  indexed row-field read, and old pending status without exposing the backing
  association-list representation.
- The bound template begins each visible row with template-owned literal text
  via `(ui-text "TODO:")`, before its existing description and status fields.
  Its runtime metadata now owns an exact-size dynamically allocated instruction
  array and exact-length literal strings; there is no per-template instruction
  or `ui-text` literal cap. The imported source path remains independently
  bounded to 64 rows, 255 bytes per source row, and 4096 bytes per file.
- The checked-in `.vscode/silos-api-stubs.lisp` gives Alive editor metadata for
  the proposed public APIs. `silos_alive_api_stubs` compares its public names
  and reviewed API-document fingerprint under CTest, so API edits fail until
  the prelude is reviewed and refreshed.
- The code-layout refactor is complete. Portable ownership is split across
  `Shell/Events`, `FreeRTOS/QueueRuntime`, `Runtime` state/bootstrap/event
  pumping, `Store`, and a `UI/Renderer` consuming `UI/PlatformSurface`.
  Browser startup, surface projection, and store-init loading live under
  `Platform/Browser`. `uLisp/Extension.cpp` aggregates interface-specific
  `.inc` fragments and `BuiltinEntries.inc`; the uLisp vendor tree contains a
  seven-line semantic hook diff, the FreeRTOS vendor tree remains unchanged,
  and CMake contains build composition rather than embedded runtime code.
  CTest passed both tests (2/2) in 0.76 seconds after the move.
- Leave `stash@{0}` (`codex-migration-pre-sync-2026-08-22`) untouched.

**Next session, without beginning it:** decide and document the Lisp record
representation and validation for `store-row-add`; implement create/edit/delete
in separately delegated phases afterwards, verifying each phase against the
browser-visible bound template, followed by persistence and restart behaviour.

## Goal

Build the smallest Browser-first SilOS prototype that expresses a real uLisp
to-do application, binds language-visible values to screen locations, updates
those locations after values change, accepts semantic input to create, edit,
and delete to-do items, and persists those items across restart.

## Baseline

- Start from the full, unmodified FreeRTOS-Kernel V11.3.0 and uLisp ESP 4.9a
  source snapshots in `third-party/`.
- Commit this state before copying, adapting, or adding any FreeWisp-derived
  runtime support. That commit is the comparison point for every subsequent
  direct change to either upstream source tree.
- Keep experiment-owned code outside `third-party/`; direct edits inside those
  trees must remain reviewable with Git diff.

## Agreed code ownership and layout

- Portable SilOS behaviour lives in capability-owned modules below
  `runtime/SilOS/`: `Store`, `UI`, `Shell`, and `Runtime`. Dependency adapters
  live in sibling `uLisp` and `FreeRTOS` modules and must not take ownership of
  those portable capability semantics.
- Platform-specific implementation lives below
  `runtime/SilOS/Platform/<Target>/`; initially the only target directory is
  `Browser`. Do not create empty directories for prospective targets.
- A module owns the narrow interface it requires a platform to implement and
  names it `Platform<Capability>.h`, such as `UI/PlatformSurface.h` or
  `Store/PlatformPersistence.h`. Target implementations use names such as
  `BrowserSurface` and depend inward on those interfaces; portable modules do
  not depend outward on the Browser implementation.
- Keep third-party uLisp and FreeRTOS changes to the smallest explicit hooks
  needed to reach SilOS-owned code. The tagged unmodified baseline exists to
  make these direct changes and future upstream reconciliation reviewable; it
  does not prohibit vendor-tree changes.
- `runtime/CMakeLists.txt` performs ordinary build composition only. Runtime
  C++ implementations, uLisp built-ins, state, and garbage-collector hooks
  must live in source files, not CMake string literals.

The rationale and dependency rules are recorded in the dedicated
[code-layout discussion](../../docs/design/Discussion-Code-Layout.md).

## Sequence

1. **[done]** Commit the vendor baseline after review.
2. **[done]** Re-establish the smallest proven Browser runtime boundary from
   FreeWisp, preserving a clear separation between upstream code and
   experiment code.
3. **[done]** Import versioned startup stores into an in-memory backend at
   Browser boot, discover the one `apps/<app-name>/app.lisp` manifest,
   evaluate its `app-declare`, then load and start the declared uLisp to-do
   entry source.
4. **[done]** Implement a read-only StoreRef-backed to-do data path using the
   same volatile in-memory backend: pending-to-ready bind completion, bounded
   old snapshots, row metadata, field reads, and one StoreRef watch.
5. **[done]** Implement the smallest useful Shell boundary: later lifecycle
   delivery, application-owned staged pokes, one StoreRef watch, and bounded
   templates/UiRefs/lists/mount are proven. Semantic input actions remain out
   of scope for this proof.
6. **[done]** Render the bounded bound-template list on a real Browser surface
   and verify that the two imported rows are visibly template-driven.
   `view-browser.sh` builds through `build.sh` and serves the proof on loopback
   for this check.
7. **[done]** Move the prototype into the agreed module/platform layout,
   replace embedded CMake runtime code with ordinary SilOS-owned sources, and
   reduce direct dependency changes to the explicit uLisp integration hooks.
8. **[next]** Decide and document the Lisp record representation and
   validation for `store-row-add`.
9. **[planned]** Implement create, edit, and delete in separately delegated
   phases.
10. **[planned]** Add persistence and restart behaviour.
11. Test and measure live binding, source-loading, handlers, references, and
   storage; record where their APIs need refinement.

## Browser visual-check gate

- At the end of every future experiment phase, update `view-browser.sh` and its
  visual-check guidance when the build output or proof surface changes, then
  pause before the next phase for the user to run the script and inspect it.
- The script must build through `build.sh` and serve the staged
  `browser-surface.html` on loopback only; it is the stable manual viewing
  entry point, not a second runtime or UI path.

## Current bootstrap subset

- Browser startup preloads and traverses `runtime/store-init/` as `/store-init`.
  The exact relative filename, including extension, is the store name:
  `apps/todo/app.lisp`, `apps/todo/src/main.lisp`, and `todo/items.csv` today.
  Each `.lisp` line becomes an ordered `text` row; a `.csv` header names the
  row fields and each data row receives sequential ID/revision `1` metadata.
  The source reader restores a newline at every `text` row boundary, allowing
  ordinary `;` Lisp comments in the versioned files.
- The imported volatile catalogue is fixed-capacity and generic. Every store
  has rows with stable ID/revision metadata and bounded named string fields;
  its backend does not encode source or to-do row shapes. It is an
  experiment-owned representation, not a commitment to the final store layout.
- It temporarily discovers only exact manifest names shaped
  `apps/<app-name>/app.lisp`. This is a one-app bootstrap shortcut, not the
  future catalogue design.
- A manifest is evaluated directly and is trusted to contain only its one
  documented `app-declare` form; restricted describe-mode validation is
  deferred.
- The native bootstrap loads the declaration's `:entry` store internally. No
  Lisp-visible `ulisp-load-store` operation exists in this increment.
- Source rows retain their insertion order only for this proof. Editable
  source will need the later row-ID/next-row linked ordering model, including
  a persisted head reference.
- The app binds the volatile `todo/items.csv` row store with documented
  `store-bind`; it sees an immediate pending StoreRef and then live
  StoreRowRefs after a bounded storage completion reaches the uLisp task.
- The app uses documented `store-watch` for that StoreRef's state change.
  The uLisp task snapshots the old pending/nil ref before updating it to ready,
  then invokes the watch once with `(live old-value)`. `app-start` remains a
  separate, unused-in-this-proof app-level handler for future Shell events.
  This increment permits one rooted StoreRef watch for the active app; watch
  removal and release on app stop/reload are still deferred.
- After `app-start`, the Shell queues one later-turn `app-initialise` event.
  The app's generic two-stage poke sequence binds its StoreRef, then declares
  and mounts the bounded StoreRef-backed to-do list. The proof currently
  renders both imported rows through its UiRef and item-template metadata,
  including ordered template-owned literal text, to
  a small Browser DOM surface. The `browser-surface.html` launcher is staged
  beside the Emscripten output and receives only renderer-resolved state and
  field values; it adds no semantic input handling or store mutation.
- This increment supports only `desc` and `status` field reads. It deliberately
  excludes row record literals, creation, updates, deletion, ordering,
  semantic input, and persistence.

## Non-goals

The experiment does not settle FreeRTOS/uLisp as the final cross-target
architecture, add later SilOS applications, or adopt the current Shell API
sketches unchanged.
