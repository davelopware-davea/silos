# Browser to-do prototype plan

**Status:** In progress; the Browser boot/load and read-only StoreRef proofs
are complete.

**Project context:** [SilOS plan](../../docs/design/SilOS_PLAN.md).

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
3. **[done]** Seed an in-memory source backend at boot, discover the one
   `apps/<app-name>/app.lisp` manifest, evaluate its `app-declare`, then load
   and start the declared uLisp to-do entry source.
4. **[done]** Implement a read-only StoreRef-backed to-do data path using the
   same volatile in-memory backend: pending-to-ready bind completion, row
   metadata, and field reads.
5. Implement the smallest useful Shell boundary: templates/UiRefs, semantic
   input dispatch, and handlers.
6. Add create, edit, delete, and restart
   persistence.
7. Test and measure live binding, source-loading, handlers, references, and
   storage; record where their APIs need refinement.

## Current bootstrap subset

- The native bootstrap seeds read-only source rows in order for
  `apps/todo/app.lisp` and `apps/todo/src/main`.
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
- The app binds the volatile `todo/items` row store with documented
  `store-bind`; it sees an immediate pending StoreRef and then live
  StoreRowRefs after a bounded storage completion reaches the uLisp task.
- This increment supports only `desc` and `status` field reads. It deliberately
  excludes row record literals, creation, updates, deletion, ordering, UI, and
  persistence.

## Non-goals

The experiment does not settle FreeRTOS/uLisp as the final cross-target
architecture, add later SilOS applications, or adopt the current Shell API
sketches unchanged.
