# Browser to-do prototype plan

**Status:** In progress; the Browser boot/load, read-only StoreRef, and bounded
lifecycle/poke/UI-list-render proofs are complete. The current lifecycle/UI/
cross-reference work is uncommitted.

**Project context:** [SilOS plan](../../docs/design/SilOS_PLAN.md).

**Relevant API sketch:** [proposed UI API](../../docs/design/API-UI.md).

## Current status and handoff

- Work only in the authoritative `C:\Users\dave\src\SilOS` checkout, on
  `codex/browser-todo-prototype`. A prior session accidentally continued in a
  stale OneDrive checkout; its content was reconciled into `src` by
  Git-normalised hashes and verified against committed history through
  `d992ad6 design: add bounded UI API proposal`, plus the current uncommitted
  lifecycle/UI/cross-reference work.
- The proof has a general queue-backed `(app-request-poke arg...)` boundary
  that delivers fresh `(poke . payload)` values, a later-turn
  `app-initialise`, application-owned `init` stages, one StoreRef watch, and
  bounded UiRef/template/list/mount rendering of the two imported CSV rows.
- Source UI forms now match the general [UI API](../../docs/design/API-UI.md).
  The API-Shell lifecycle/poke amendment is currently uncommitted. The imported
  Lisp source has 63 rows, within its fixed 64-row limit; no vendor files have
  changed. CTest passed in `C:\Users\dave\src\SilOS` in 0.26 seconds.
- Leave `stash@{0}` (`codex-migration-pre-sync-2026-08-22`) and the unrelated
  untracked `.vscode/` directory untouched.

**Next session, without beginning it:** review the uncommitted
lifecycle/UI/poke/cross-reference diff and make the next checkpoint commit;
decide and document the Lisp record representation and validation for
`store-row-add`; then implement create/edit/delete in separately delegated
phases, followed by persistence and restart behaviour.

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
6. **[next]** Decide and document the Lisp record representation and
   validation for `store-row-add`.
7. **[planned]** Implement create, edit, and delete in separately delegated
   phases.
8. **[planned]** Add persistence and restart behaviour.
9. Test and measure live binding, source-loading, handlers, references, and
   storage; record where their APIs need refinement.

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
- The app uses documented `store-ref-watch` for that StoreRef's state change.
  The uLisp task snapshots the old pending/nil ref before updating it to ready,
  then invokes the watch once with `(live old-value)`. `app-start` remains a
  separate, unused-in-this-proof app-level handler for future Shell events.
  This increment permits one rooted StoreRef watch for the active app; watch
  removal and release on app stop/reload are still deferred.
- After `app-start`, the Shell queues one later-turn `app-initialise` event.
  The app's generic two-stage poke sequence binds its StoreRef, then declares
  and mounts the bounded StoreRef-backed to-do list. The proof renders both
  imported rows through its UiRef and item-template metadata; it adds no
  semantic input handling.
- This increment supports only `desc` and `status` field reads. It deliberately
  excludes row record literals, creation, updates, deletion, ordering,
  semantic input, and persistence.

## Non-goals

The experiment does not settle FreeRTOS/uLisp as the final cross-target
architecture, add later SilOS applications, or adopt the current Shell API
sketches unchanged.
